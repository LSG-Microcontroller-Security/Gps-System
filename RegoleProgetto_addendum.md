# Addendum Regole Progetto

## Guard Rail Su Test, Mocking E Macro

- Non mockare mai le `activity` (`mf_activity_*`).
- Mockare solo dipendenze infrastrutturali: `repository`, adapter, interfacce hardware o device access.
- Nei test della business logic usare sempre questo pattern:
  - `activity` reale;
  - `repository` mockato;
  - business logic testata sopra `activity` reale.
- Se una `activity` viene mockata, il test non e' conforme alle regole architetturali del progetto.

## Macro Di Test E Debug Nei Progetti Visual Studio

- Non hardcodare `_ON_MOCKING_TESTS` e `_DEBUG_FOR_SERIAL` dentro file versionati `*.vcxproj`.
- Nel codice applicativo non introdurre o mantenere branch condizionali su `_ON_MOCKING_TESTS`, in coerenza con la sezione `Macro E Debug` del file principale.
- Le macro di build temporanee devono stare fuori dal file di progetto versionato, ad esempio:
  - in impostazioni locali non versionate;
  - in command line di build;
  - in configurazioni locali dedicate.
- Se queste macro compaiono nel `vcxproj`, vanno rimosse prima di chiudere il lavoro.

## Checklist Obbligatoria Prima Di Concludere

- Verificare assenza di mock `activity` nel codice test:
  - cercare `Mock<mf_activity_` o pattern equivalenti.
- Verificare assenza di macro hardcoded nei `vcxproj`:
  - cercare `_ON_MOCKING_TESTS=` e `_DEBUG_FOR_SERIAL=`.
- Se trovi una violazione, correggere prima della chiusura.
