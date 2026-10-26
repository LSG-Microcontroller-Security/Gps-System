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
# Reuse the mismatch window for development approval and protected-branch errors.
# Copying a hash never changes a checkout or authorizes the build.
function Show-LibraryRevisionMismatch([string]$Repository, [string]$Required, [string]$Actual, [string]$Branch, [bool]$allow_local_revision = $false) {
    $message = "The firmware on '$Branch' requires a different library revision.`r`n`r`nRepository: $Repository`r`nRequired commit: $Required`r`nLocal commit: $Actual"
    if ($allow_local_revision) {
        $message += "`r`n`r`nDo you want to build with the LOCAL commit? Yes continues with the local revision and updates the manifest only if ALL linked Git libraries are clean. No cancels this build without updating the manifest."
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
        $form.Text = 'GPM - Library version mismatch'
        $form.ClientSize = New-Object System.Drawing.Size(680, 420)
        $form.StartPosition = 'CenterScreen'
        $form.FormBorderStyle = 'FixedDialog'
        $form.MaximizeBox = $false
        $form.MinimizeBox = $false
        $form.TopMost = $true
        $label = New-Object System.Windows.Forms.TextBox
        $label.Multiline = $true
        $label.ReadOnly = $true
        $label.ScrollBars = 'Vertical'
        $label.SetBounds(16, 16, 648, 320)
        $label.Text = $message
        $form.Controls.Add($label)
        $copy = New-Object System.Windows.Forms.Button
        $copy.Text = 'Copy required hash'
        $copy.SetBounds(16, 360, 180, 32)
        $required_hash = $Required
        $copy.Add_Click({ [System.Windows.Forms.Clipboard]::SetText($required_hash) }.GetNewClosure())
        $form.Controls.Add($copy)
        if ($allow_local_revision) {
            $proceed = New-Object System.Windows.Forms.Button
            $proceed.Text = 'Yes - use local hash'
            $proceed.SetBounds(340, 360, 180, 32)
            $proceed.DialogResult = [System.Windows.Forms.DialogResult]::Yes
            $form.Controls.Add($proceed)
        }
        $close = New-Object System.Windows.Forms.Button
        $close.Text = if ($allow_local_revision) { 'No - cancel build' } else { 'Close' }
        $close.SetBounds(540, 360, 124, 32)
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
try {
    $repo = Invoke-Git $SketchPath @('rev-parse', '--show-toplevel')
    $gitDir = Invoke-Git $repo @('rev-parse', '--absolute-git-dir')
    $branch = Invoke-Git $repo @('branch', '--show-current')
    $manifestFile = Join-Path $repo '.gpm/avr-libraries.json'
    if (-not (Test-Path -LiteralPath $manifestFile)) {
        throw "Missing AVR dependency manifest: $manifestFile"
    }
    $manifest = Get-Content -LiteralPath $manifestFile -Raw -Encoding UTF8 | ConvertFrom-Json
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
    $firmwareStatus = Invoke-Git $repo @('status', '--porcelain', '--', '.', ':(exclude)**/GitVersion.generated.h', ':(exclude,glob)**/*.vcxproj')
    if ($firmwareStatus) { $dirtyNames.Add((Split-Path $repo -Leaf)) }
    $localRevisions = @()
    $pendingUpdates = @()
    $revisionErrors = @()
    foreach ($source in $repositories) {
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
        $status = Invoke-Git $libraryRoot @('status', '--porcelain')
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
        if ($source.ref -ne $head) {
            if (-not $developmentBranch) {
                [void](Show-LibraryRevisionMismatch -Repository $source.url -Required $source.ref -Actual $head -Branch $branch)
                $revisionErrors += "Library version mismatch on protected/non-development branch '$branch' for $($source.url). Required $($source.ref), local $head. Manually check out the required library commit to rebuild; for newer libraries, update the manifest via feature/develop -> release -> main."
            }
            else {
                $pendingUpdates += [pscustomobject]@{ source = $source; head = $head }
            }
        }
        else { Write-Host "AVR libraries unchanged: $head" }
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
    if ($revisionErrors.Count -gt 0) {
        throw ($revisionErrors -join "`n")
    }
    # Obtain every decision before changing any manifest entry. A later No must
    # not leave an earlier repository partially updated.
    foreach ($mismatch in $pendingUpdates) {
        if (-not (Show-LibraryRevisionMismatch -Repository $mismatch.source.url -Required $mismatch.source.ref -Actual $mismatch.head -Branch $branch -allow_local_revision $true)) {
            throw 'Build cancelled by user: local library revision was not approved.'
        }
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
