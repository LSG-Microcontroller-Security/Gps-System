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

## Regola Operativa Su File `.ino` E Repository

- Il file `.ino` non deve chiamare direttamente metodi dei repository.
- Il file `.ino` deve chiamare solo metodi pubblici della business logic.
- Se nel loop serve una sequenza applicativa (esempio: contatori, soglie, ripetizioni), il loop puo' stare nel `.ino`, ma le azioni verso i repository devono passare dalla business logic.
- In pratica:
  - nel `.ino`: orchestrazione del loop e chiamate alla business logic;
  - nella business logic: chiamate ai repository.

## Checklist Obbligatoria Prima Di Concludere

- Verificare assenza di mock `activity` nel codice test:
  - cercare `Mock<mf_activity_` o pattern equivalenti.
- Verificare assenza di macro hardcoded nei `vcxproj`:
  - cercare `_ON_MOCKING_TESTS=` e `_DEBUG_FOR_SERIAL=`.
- Verificare che il `.ino` non chiami repository direttamente:
  - cercare invocazioni dirette su oggetti repository dentro il file `.ino`.
- Se trovi una violazione, correggere prima della chiusura.
