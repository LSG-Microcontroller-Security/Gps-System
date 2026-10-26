# This script is installed beside GenerateGitVersion.bat. Avoid command-line
# arguments: Visual Micro's prebuild command processing may drop a trailing
# parameter. Resolve both paths from the script's actual directory instead.
$SketchPath = $PSScriptRoot
$GeneratedHeader = Join-Path $SketchPath 'GitVersion.generated.tmp'
$ErrorActionPreference = 'Stop'
$localLibraryLinkError = 'GPM_AVR_LOCAL_LIBRARY_LINK_ERROR'

function Invoke-Git([string]$Location, [string[]]$Arguments) {
    $value = & git -C $Location @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Git command failed in $($Location): git $($Arguments -join ' ')`n$($value -join "`n")"
    }
    return ($value -join "`n").Trim()
}
# Read the live official branch tip without fetching objects or changing refs.
# The timeout and credential settings keep unattended builds from waiting for input.
function get_remote_library_revision([string]$library_root) {
    $git_process = $null
    try {
        $start_info = New-Object System.Diagnostics.ProcessStartInfo
        $start_info.FileName = 'git'
        $start_info.Arguments = '-c core.askPass= -c credential.interactive=false ls-remote --symref origin HEAD refs/heads/main refs/heads/master'
        $start_info.WorkingDirectory = $library_root
        $start_info.UseShellExecute = $false
        $start_info.CreateNoWindow = $true
        $start_info.RedirectStandardOutput = $true
        $start_info.RedirectStandardError = $true
        $start_info.RedirectStandardInput = $true
        $start_info.EnvironmentVariables['GIT_TERMINAL_PROMPT'] = '0'
        $start_info.EnvironmentVariables['GCM_INTERACTIVE'] = 'Never'
        $start_info.EnvironmentVariables['GIT_ASKPASS'] = ''
        $start_info.EnvironmentVariables['SSH_ASKPASS_REQUIRE'] = 'never'
        $start_info.EnvironmentVariables['SSH_ASKPASS'] = ''
        $git_process = New-Object System.Diagnostics.Process
        $git_process.StartInfo = $start_info
        if (-not $git_process.Start()) { throw 'Cannot start Git to verify the remote library revision.' }
        $git_process.StandardInput.Close()
        $stdout_task = $git_process.StandardOutput.ReadToEndAsync()
        $stderr_task = $git_process.StandardError.ReadToEndAsync()
        if (-not $git_process.WaitForExit(15000)) {
            $git_process.Kill()
            throw 'Remote library revision lookup timed out after 15 seconds.'
        }
        if (-not $stdout_task.Wait(1000) -or -not $stderr_task.Wait(1000)) {
            throw 'Cannot read the remote library revision response within the timeout.'
        }
        $stdout = $stdout_task.Result
        $stderr = $stderr_task.Result.Trim()
        if ($git_process.ExitCode -ne 0) {
            if (-not $stderr) { $stderr = 'Git could not query origin.' }
            throw $stderr
        }
        $remote_refs = @{}
        $preferred_branch = ''
        foreach ($line in ($stdout -split "\r?\n")) {
            if ($line -match '^ref:\s+refs/heads/(main|master)\s+HEAD$') {
                $preferred_branch = $Matches[1].ToLowerInvariant()
            }
            elseif ($line -match '^([a-fA-F0-9]{40})\s+refs/heads/(main|master)$') {
                $remote_refs[$Matches[2].ToLowerInvariant()] = $Matches[1].ToLowerInvariant()
            }
        }
        if ($preferred_branch -and $remote_refs.ContainsKey($preferred_branch)) {
            $remote_branch = $preferred_branch
        }
        elseif ($remote_refs.ContainsKey('main')) { $remote_branch = 'main' }
        elseif ($remote_refs.ContainsKey('master')) { $remote_branch = 'master' }
        else { throw 'The library origin has no main or master branch to use as the official reference.' }
        return [pscustomobject]@{ branch = $remote_branch; ref = $remote_refs[$remote_branch]; error = '' }
    }
    catch {
        return [pscustomobject]@{ branch = ''; ref = ''; error = $_.Exception.Message }
    }
    finally {
        if ($null -ne $git_process) { $git_process.Dispose() }
    }
}
# The GUI is available only for a local, interactive Windows build.
# Keeping this as a separate function also allows headless regression tests
# to substitute a decision without weakening the production safety checks.
function Request-DirtyBuildApproval([string]$Message) {
    if ($env:CI -or $env:GITHUB_ACTIONS -or -not [Environment]::UserInteractive -or
        [Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw "$Message`r`nBuild stopped: interactive confirmation is unavailable."
    }
    try {
        Add-Type -AssemblyName System.Windows.Forms
        $answer = [System.Windows.Forms.MessageBox]::Show(
            "$Message`r`n`r`nThese local changes are not committed to Git and may make this build unreproducible.`r`n`r`nDo you want to continue the build anyway?",
            'GPM - WARNING: Uncommitted Git Changes',
            [System.Windows.Forms.MessageBoxButtons]::YesNo,
            [System.Windows.Forms.MessageBoxIcon]::Warning,
            [System.Windows.Forms.MessageBoxDefaultButton]::Button2
        )
        return ($answer -eq [System.Windows.Forms.DialogResult]::Yes)
    }
    catch { throw "$Message`r`nCannot display confirmation: $($_.Exception.Message)" }
}
# Show the local, manifest and live remote commits together. A newer remote tip
# is advisory; a protected-branch manifest mismatch remains a build blocker.
function Show-LibraryRevisionMismatch([string]$Repository, [string]$Required, [string]$Actual, [string]$Branch, [bool]$allow_local_revision = $false, [string]$remote_branch = '', [string]$remote_ref = '', [string]$remote_error = '') {
    $message = "Repository: $Repository`r`nFirmware branch: $Branch`r`n`r`nLocal commit (A): $Actual`r`nManifest commit (B): $Required"
    if ($remote_error) {
        $message += "`r`nLatest official remote commit (C): NOT VERIFIED`r`nReason: $remote_error"
    }
    else {
        $message += "`r`nLatest origin/$remote_branch commit (C): $remote_ref"
    }
    if ($Required -ne $Actual) {
        $message += "`r`n`r`nMANIFEST MISMATCH: the local library commit differs from the revision required by this firmware."
    }
    if (-not $remote_error -and $remote_ref -ne $Actual) {
        $message += "`r`n`r`nREMOTE DIFFERENCE: the local library commit differs from the latest origin/$remote_branch commit. This does not by itself make the pinned firmware revision invalid."
    }
    $message += "`r`n`r`nCopy the manifest or remote hash to inspect the corresponding revision before deciding. The manifest declares a required commit; it does not certify a successful build or deployment."
    if ($allow_local_revision) {
        $message += "`r`n`r`nDo you want to build with the LOCAL commit (A)? Yes keeps the local checkout. Only approved development manifest mismatches update the manifest, and only if ALL linked Git libraries are clean. No cancels without updating the manifest."
    }
    else {
        $message += "`r`n`r`nBuild stopped: this is a protected or non-development branch.`r`nTo rebuild this release, copy the required commit and inspect it in SourceTree. After safeguarding local changes, manually check out that commit in the LIBRARY repository, then rebuild.`r`n`r`nTo release with newer libraries, update and validate the manifest on a feature/develop branch, then follow the normal release-to-main PR process."
    }
    $message += "`r`n`r`nNo automatic checkout will be performed."
    if ($env:CI -or $env:GITHUB_ACTIONS -or -not [Environment]::UserInteractive -or
        [Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        Write-Host $message
        if ($allow_local_revision) { throw 'Build stopped: interactive library revision confirmation is unavailable.' }
        return $false
    }
    try {
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName System.Drawing
        $form = New-Object System.Windows.Forms.Form
        $form.Text = 'GPM - Library revision check'
        $form.ClientSize = New-Object System.Drawing.Size(860, 520)
        $form.StartPosition = 'CenterScreen'
        $form.FormBorderStyle = 'FixedDialog'
        $form.MaximizeBox = $false
        $form.MinimizeBox = $false
        $form.TopMost = $true
        $label = New-Object System.Windows.Forms.TextBox
        $label.Multiline = $true
        $label.ReadOnly = $true
        $label.ScrollBars = 'Vertical'
        $label.SetBounds(16, 16, 828, 370)
        $label.Text = $message
        $form.Controls.Add($label)
        $copy = New-Object System.Windows.Forms.Button
        $copy.Text = 'Copy manifest hash (B)'
        $copy.SetBounds(16, 402, 196, 32)
        $required_hash = $Required
        $copy.Add_Click({ [System.Windows.Forms.Clipboard]::SetText($required_hash) }.GetNewClosure())
        $form.Controls.Add($copy)
        $copy_remote = New-Object System.Windows.Forms.Button
        $copy_remote.Text = 'Copy remote hash (C)'
        $copy_remote.SetBounds(228, 402, 196, 32)
        $copy_remote.Enabled = (-not $remote_error -and -not [string]::IsNullOrWhiteSpace($remote_ref))
        $remote_hash = $remote_ref
        $copy_remote.Add_Click({ [System.Windows.Forms.Clipboard]::SetText($remote_hash) }.GetNewClosure())
        $form.Controls.Add($copy_remote)
        if ($allow_local_revision) {
            $proceed = New-Object System.Windows.Forms.Button
            $proceed.Text = 'Yes - use local hash'
            $proceed.SetBounds(436, 462, 196, 32)
            $proceed.DialogResult = [System.Windows.Forms.DialogResult]::Yes
            $form.Controls.Add($proceed)
        }
        $close = New-Object System.Windows.Forms.Button
        $close.Text = if ($allow_local_revision) { 'No - cancel build' } else { 'Close' }
        $close.SetBounds(648, 462, 196, 32)
        $close.DialogResult = [System.Windows.Forms.DialogResult]::Cancel
        $form.CancelButton = $close
        $form.AcceptButton = $close
        $form.Controls.Add($close)
        return ($form.ShowDialog() -eq [System.Windows.Forms.DialogResult]::Yes)
    }
    catch {
        Write-Warning "Cannot display library mismatch window: $($_.Exception.Message)"
        Write-Host $message
        if ($allow_local_revision) { throw 'Build stopped: cannot display library revision confirmation.' }
        return $false
    }
    finally {
        if ($null -ne $form) { $form.Dispose() }
    }
}
function Normalize-Remote([string]$Remote) {
    $url = $Remote.Trim()
    if ($url -match '^git@github\.com:(.+)$') { $url = "https://github.com/$($Matches[1])" }
    elseif ($url -match '^ssh://git@github\.com/(.+)$') { $url = "https://github.com/$($Matches[1])" }
    return ($url -replace '\.git$', '').TrimEnd('/')
}
function Write-Utf8([string]$Path, [string]$Text) {
    [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.UTF8Encoding($false)))
}
# Inspecting a hash in a Git client may also change the local checkout. Never
# publish decisions made against an earlier HEAD, branch or worktree state.
function assert_repository_unchanged([string]$repository_root, [string]$expected_head, [string]$expected_branch, [string]$expected_status) {
    $current_head = Invoke-Git $repository_root @('rev-parse', 'HEAD')
    $current_branch = Invoke-Git $repository_root @('branch', '--show-current')
    $current_status = Invoke-Git $repository_root @('status', '--porcelain', '--untracked-files=all')
    if ($current_head -cne $expected_head -or $current_branch -cne $expected_branch -or $current_status -cne $expected_status) {
        throw "Repository changed during prebuild verification: $repository_root. Rebuild to verify the new revision and working tree."
    }
}
try {
    $repo = Invoke-Git $SketchPath @('rev-parse', '--show-toplevel')
    $gitDir = Invoke-Git $repo @('rev-parse', '--absolute-git-dir')
    $branch = Invoke-Git $repo @('branch', '--show-current')
    $firmware_head = Invoke-Git $repo @('rev-parse', 'HEAD')
    $manifestFile = Join-Path $repo '.gpm/avr-libraries.json'
    if (-not (Test-Path -LiteralPath $manifestFile)) {
        throw "Missing AVR dependency manifest: $manifestFile"
    }
    $manifest_snapshot = Get-Content -LiteralPath $manifestFile -Raw -Encoding UTF8
    $manifest = $manifest_snapshot | ConvertFrom-Json
    if ($manifest.schemaVersion -ne 1) { throw 'Unsupported AVR dependency manifest schema' }
    $repositories = @($manifest.gitRepositories | Where-Object { $null -ne $_ })
    $mappingFile = Join-Path $gitDir 'gpm-avr-library-paths.json'
    if ($repositories.Count -gt 0 -and -not (Test-Path -LiteralPath $mappingFile)) {
        throw $localLibraryLinkError
    }
    $localSources = @()
    if (Test-Path -LiteralPath $mappingFile) {
        $mapping = Get-Content -LiteralPath $mappingFile -Raw -Encoding UTF8 | ConvertFrom-Json
        $localSources = @($mapping.sources | Where-Object { $null -ne $_ })
    }
    $changed = $false
    $developmentBranch = $branch -match '^(develop|feature/.+|fix/.+|bugfix/.+|task/.+|hotfix/.+)$'
    $hasDirtyLibraries = $false
    $hasUnofficialLibraryBranch = $false
    $dirtyNames = New-Object 'System.Collections.Generic.List[string]'
    $firmwareStatus = Invoke-Git $repo @('status', '--porcelain', '--untracked-files=all')
    if ($firmwareStatus) { $dirtyNames.Add((Split-Path $repo -Leaf)) }
    $localRevisions = @()
    $pendingUpdates = @()
    $revisionErrors = @()
    $revision_checks = @()
    foreach ($source in $repositories) {
        if ([string]$source.ref -notmatch '^[a-fA-F0-9]{40}$') { throw "Invalid manifest Git SHA for $($source.url)" }
        $candidates = @($localSources | Where-Object {
            (Normalize-Remote $_.url) -ieq (Normalize-Remote $source.url)
        })
        if ($candidates.Count -ne 1) {
            throw $localLibraryLinkError
        }
        try { $libraryRoot = [System.IO.Path]::GetFullPath($candidates[0].path) }
        catch { throw $localLibraryLinkError }
        if (-not (Test-Path -LiteralPath $libraryRoot -PathType Container)) {
            throw $localLibraryLinkError
        }
        try { $actualRoot = Invoke-Git $libraryRoot @('rev-parse', '--show-toplevel') }
        catch {
            if (-not (Test-Path -LiteralPath (Join-Path $libraryRoot '.git'))) { throw $localLibraryLinkError }
            throw
        }
        if ([System.IO.Path]::GetFullPath($actualRoot) -ine $libraryRoot) {
            throw $localLibraryLinkError
        }
        $origin = Invoke-Git $libraryRoot @('remote', 'get-url', 'origin')
        if ((Normalize-Remote $origin) -ine (Normalize-Remote $source.url)) {
            throw "Wrong Git remote for $libraryRoot ($origin, expected $($source.url))"
        }
        $status = Invoke-Git $libraryRoot @('status', '--porcelain', '--untracked-files=all')
        if ($status) {
            $dirtyNames.Add((Split-Path $libraryRoot -Leaf))
            $hasDirtyLibraries = $true
        }
        $head = Invoke-Git $libraryRoot @('rev-parse', 'HEAD')
        if ($head -notmatch '^[a-fA-F0-9]{40}$') { throw "Invalid local Git SHA for $libraryRoot" }
        # This library's branch must be independent from the firmware branch.
        $libraryBranch = Invoke-Git $libraryRoot @('branch', '--show-current')
        if ($libraryBranch) {
            if ($libraryBranch -notin @('main', 'master')) { $hasUnofficialLibraryBranch = $true }
        }
        else {
            # Detached HEAD: the pinned commit is official if reachable from
            # local or remote main/master, without requiring a network fetch.
            $containing = Invoke-Git $libraryRoot @('branch', '--all', '--contains', 'HEAD', '--format=%(refname:short)')
            $officialBranches = @($containing -split "\r?\n" | Where-Object { $_ -in @('main', 'master', 'origin/main', 'origin/master') })
            if ($officialBranches.Count -eq 0) { $hasUnofficialLibraryBranch = $true }
        }
        $localRevisions += [pscustomobject]@{ url = $source.url; ref = $head }
        $revision_checks += [pscustomobject]@{ source = $source; head = $head; library_root = $libraryRoot; branch = $libraryBranch; status = $status }
        if ($source.ref -ne $head) {
            if (-not $developmentBranch) {
                $revisionErrors += "Library version mismatch on protected/non-development branch '$branch' for $($source.url). Required $($source.ref), local $head. Manually check out the required library commit to rebuild; for newer libraries, update the manifest via feature/develop -> release -> main."
            }
            else {
                $pendingUpdates += [pscustomobject]@{ source = $source; head = $head }
            }
        }
        else { Write-Host "AVR library commit matches manifest: $head" }
    }
    # Check all participating repositories together, before updating the manifest.
    if ($dirtyNames.Count -gt 0) {
        $names = ($dirtyNames | Select-Object -Unique) -join "`r`n- "
        $message = "WARNING! GPM detected uncommitted changes in the following Git repositories:`r`n- $names"
        if (-not $developmentBranch) {
            throw "$message`r`nBuild stopped: this is a protected or non-development branch."
        }
        if (-not (Request-DirtyBuildApproval $message)) {
            throw 'Build cancelled by user: uncommitted Git changes were not approved.'
        }
        Write-Warning "Uncommitted Git changes approved for build: $($dirtyNames -join ', ')"
    }
    # Query live remote tips only after dirty checks have allowed this build.
    # Every decision is collected before any manifest entry can be changed.
    foreach ($revision_check in $revision_checks) {
        $remote_revision = get_remote_library_revision $revision_check.library_root
        $manifest_mismatch = $revision_check.source.ref -ne $revision_check.head
        $remote_mismatch = -not $remote_revision.error -and $remote_revision.ref -ne $revision_check.head
        if ($remote_revision.error -or $manifest_mismatch -or $remote_mismatch) {
            $allow_local_revision = $developmentBranch -or -not $manifest_mismatch
            $approved = Show-LibraryRevisionMismatch -Repository $revision_check.source.url -Required $revision_check.source.ref -Actual $revision_check.head -Branch $branch -allow_local_revision $allow_local_revision -remote_branch $remote_revision.branch -remote_ref $remote_revision.ref -remote_error $remote_revision.error
            # A dialog result can never override protected manifest consistency.
            if (-not $allow_local_revision) { continue }
            if (-not $approved) {
                throw 'Build cancelled by user: local library revision was not approved.'
            }
            Write-Warning "Local library revision approved: $($revision_check.source.url) @ $($revision_check.head)"
        }
        else {
            Write-Host "AVR library commit matches latest origin/$($remote_revision.branch): $($remote_revision.ref)"
        }
    }
    if ($revisionErrors.Count -gt 0) {
        throw ($revisionErrors -join "`n")
    }
    # All approvals refer to these snapshots. A checkout, commit, new dirty
    # file or manifest edit performed while a window was open requires a retry.
    assert_repository_unchanged $repo $firmware_head $branch $firmwareStatus
    foreach ($revision_check in $revision_checks) {
        assert_repository_unchanged $revision_check.library_root $revision_check.head $revision_check.branch $revision_check.status
    }
    if (-not (Test-Path -LiteralPath $manifestFile) -or (Get-Content -LiteralPath $manifestFile -Raw -Encoding UTF8) -cne $manifest_snapshot) {
        throw 'AVR dependency manifest changed during prebuild verification. Rebuild to verify the new manifest.'
    }
    # Never record a partial set of local commits while any library has
    # uncommitted changes. Only clean development checkouts update the manifest.
    if ($hasDirtyLibraries) {
        if ($pendingUpdates.Count -gt 0) {
            Write-Warning 'The AVR library manifest was NOT updated because local libraries are dirty. Commit the libraries, then rebuild before opening a firmware PR.'
        }
    }
    else {
        foreach ($update in $pendingUpdates) {
            Write-Host "AVR libraries: $($update.source.url) $($update.source.ref) -> $($update.head)"
            $update.source.ref = $update.head
            $changed = $true
        }
    }
    if ($changed) {
        $json = $manifest | ConvertTo-Json -Depth 30
        Write-Utf8 $manifestFile ($json + "`n")
        Write-Host 'Updated .gpm/avr-libraries.json: commit it with your firmware before opening the PR.'
    }
    # The generated header is ignored by Git. Its paired version references
    # are reproducible from the committed firmware and library manifest.
    $versions = @()
    foreach ($revision in $localRevisions) { $versions += "$($revision.url)@$($revision.ref)" }
    foreach ($lib in @($manifest.arduinoLibraryManager | Where-Object { $null -ne $_ })) {
        if ([string]::IsNullOrWhiteSpace($lib.version)) { throw "Unpinned Arduino Library Manager version: $($lib.name)" }
        $versions += "$($lib.name)@$($lib.version)"
    }
    # Same seven-character interface as GIT_VERSION, including on development branches.
    # One Git repository: its abbreviated commit. Multiple dependencies: a
    # deterministic seven-character fingerprint of all pinned references.
    # Always use a real Git SHA when exactly one Git repository is pinned.
    # Multiple dependencies (or none) use a deterministic SHA256 fingerprint;
    # an empty dependency set has the SHA256(empty string) fingerprint.
    if ($versions.Count -eq 1 -and $repositories.Count -eq 1) {
        $shortVersion = ([string]$localRevisions[0].ref).Substring(0, 7).ToLowerInvariant()
    }
    else {
        $data = [System.Text.Encoding]::UTF8.GetBytes(($versions -join ';'))
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try { $digest = $sha256.ComputeHash($data) }
        finally { $sha256.Dispose() }
        $shortVersion = [System.BitConverter]::ToString($digest).Replace('-', '').Substring(0, 7).ToLowerInvariant()
    }
    # Library origin is determined by library checkout branch(es), not firmware.
    # A combined version is unofficial if any Git library is unofficial.
    if ($hasUnofficialLibraryBranch) { $shortVersion += '*' }
    if ($hasDirtyLibraries) { $shortVersion += 'X' }
    $headerLine = "#define GIT_LIBRARIES_VERSION `"$shortVersion`"`n"
    [System.IO.File]::AppendAllText($GeneratedHeader, $headerLine, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host "GIT_LIBRARIES_VERSION: $shortVersion"
    Write-Host "AVR library revision(s): $($versions -join '; ')"
    exit 0
}
catch {
    if ($_.Exception.Message -eq $localLibraryLinkError) {
        [Console]::Error.WriteLine("ERROR: AVR local libraries not linked or path invalid.")
        [Console]::Error.WriteLine("Run Git Project Manager (GPM) to configure library paths, then rebuild.")
    }
    else {
        [Console]::Error.WriteLine("GPM AVR library version error: $($_.Exception.Message)")
    }
    exit 1
}
