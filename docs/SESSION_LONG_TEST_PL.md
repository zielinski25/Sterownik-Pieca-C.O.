# TEST DŁUGIEJ SESJI — Android v0.10.15

Cel: potwierdzić, że aplikacja nie wraca do logowania z powodu chwilowego błędu sieci ani wygaśnięcia 1-godzinnego ID tokena.

## Oczekiwane
- Po zwykłym zamknięciu i ponownym uruchomieniu: brak logowania.
- Po kilku godzinach: brak logowania.
- Przy błędzie sieci: komunikat o oczekiwaniu, ale przyciski logowania nie pojawiają się jako wymuszenie.
- Po powrocie sieci: Firebase odświeża ID token automatycznie i Dashboard wraca do LIVE.
- Po realnym unieważnieniu refresh tokena przez Firebase: dopiero wtedy pojawia się logowanie.
