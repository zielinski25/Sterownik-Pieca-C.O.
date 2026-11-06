/* =========================================================================
   TO BE — baza treści
   Materiał opracowany na podstawie:
   • zrzutu e-dziennika (kartkówka 10.11.2026, język angielski),
   • zdjęć podręcznika i zeszytu ćwiczeń "Together 2026, klasa 4"
     (Unit 1: s. 10–11, 13–16; Unit 2: s. 18–21; Powtórka / Catch-up,
      Przygotowanie do sprawdzianu),
   • plików PDF SB / WB / ZP z Dysku Google.
   Wszystkie zdania używają słownictwa i postaci z książki.
   ========================================================================= */
(function (root) {
  "use strict";

  const BE = [
    { en: "I",    pl: "ja",       form: "am",  short: "I'm",    negLong: "I am not",    negShort: "I'm not",    question: "Am I",       yes: "Yes, you are.",    no: "No, you aren't." },
    { en: "You",  pl: "ty",       form: "are", short: "You're", negLong: "You are not", negShort: "You aren't", question: "Are you",    yes: "Yes, I am.",       no: "No, I'm not." },
    { en: "He",   pl: "on",       form: "is",  short: "He's",   negLong: "He is not",   negShort: "He isn't",   question: "Is he",      yes: "Yes, he is.",      no: "No, he isn't." },
    { en: "She",  pl: "ona",      form: "is",  short: "She's",  negLong: "She is not",  negShort: "She isn't",  question: "Is she",     yes: "Yes, she is.",     no: "No, she isn't." },
    { en: "It",   pl: "ono/to",   form: "is",  short: "It's",   negLong: "It is not",   negShort: "It isn't",   question: "Is it",      yes: "Yes, it is.",      no: "No, it isn't." },
    { en: "We",   pl: "my",       form: "are", short: "We're",  negLong: "We are not",  negShort: "We aren't",  question: "Are we",     yes: "Yes, you are.",    no: "No, you aren't." },
    { en: "You",  pl: "wy",       form: "are", short: "You're", negLong: "You are not", negShort: "You aren't", question: "Are you",    yes: "Yes, we are.",     no: "No, we aren't." },
    { en: "They", pl: "oni/one",  form: "are", short: "They're",negLong: "They are not",negShort: "They aren't",question: "Are they",   yes: "Yes, they are.",   no: "No, they aren't." }
  ];

  /* ---- Laboratorium: maszyna zdań (substitution table) + „Find Someone Who" ----
     Inspiracja: karty „subject + be + ending" oraz klasowa zabawa w pytania
     Yes/No z krótkimi odpowiedziami (Teach-This, British Council). */
  const MACHINE = {
    subjects: [
      { word: "I", bi: 0, pl: "ja" },
      { word: "You", bi: 1, pl: "ty" },
      { word: "He", bi: 2, pl: "on" },
      { word: "She", bi: 3, pl: "ona" },
      { word: "It", bi: 4, pl: "ono/to" },
      { word: "We", bi: 5, pl: "my" },
      { word: "They", bi: 7, pl: "oni/one" },
      { word: "Robbie", bi: 2, pl: "robot z podręcznika" },
      { word: "Lena", bi: 3, pl: "koleżanka z klasy" },
      { word: "Mickey", bi: 2, pl: "kolega z klasy" },
      { word: "Ms Collins", bi: 3, pl: "nauczycielka" },
      { word: "My friends", bi: 7, pl: "moi koledzy" }
    ],
    endings: ["eleven.", "ten.", "a student.", "my friend.", "from Poland.", "Polish.",
              "at school.", "in the park.", "happy.", "cool.", "clever.", "a robot.",
              "my teacher.", "very tall.", "in the UK.", "shy.", "amazing.", "in Class 4."],
    /* Dane postaci są ZMYŚLONE na potrzeby zabawy — ćwiczymy wyłącznie
       krótkie odpowiedzi „Yes, I am. / No, I'm not.". */
    people: [
      { name: "Robbie", age: 10, from: "the UK", job: "a robot" },
      { name: "Lena", age: 11, from: "Poland", job: "a student" },
      { name: "Mickey", age: 10, from: "Brazil", job: "a student" },
      { name: "Ana", age: 11, from: "Brazil", job: "a student" },
      { name: "Ms Collins", age: 32, from: "the UK", job: "a teacher" }
    ],
    questions: [
      { q: "Are you eleven?", test: "age11" },
      { q: "Are you ten?", test: "age10" },
      { q: "Are you from Poland?", test: "poland" },
      { q: "Are you from Brazil?", test: "brazil" },
      { q: "Are you a robot?", test: "robot" },
      { q: "Are you a teacher?", test: "teacher" },
      { q: "Are you a student?", test: "student" }
    ]
  };

  const RULES = [
    { icon: "🧩", title: "Trzy formy", text: "<b>I → am</b>, <b>he / she / it → is</b>, <b>you / we / they → are</b>. Zaimki osobowe zastępują osobę lub rzecz: <i>Mark is a student. → He is a student.</i>" },
    { icon: "➕", title: "Twierdzenie", text: "Najpierw <b>osoba</b>, potem <b>be</b>: <i>I am from Poland. She is eleven. They are cool.</i> Krótka forma: <i>I'm, he's, we're, they're</i>." },
    { icon: "➖", title: "Przeczenie", text: "Dodaj <b>not</b> po czasowniku <b>be</b>: <i>is not → isn't</i>, <i>are not → aren't</i>, <i>I am not → I'm not</i>. Uwaga: <b>amn't</b> nie istnieje!" },
    { icon: "❓", title: "Pytanie", text: "Czasownik <b>be</b> wędruje <b>na początek</b>: <i>She is Polish. → Is she Polish?</i> Nie dodajemy <i>do / does</i>." },
    { icon: "💬", title: "Krótkie odpowiedzi", text: "<i>Are you ten? — Yes, I am. / No, I'm not.</i> W twierdzącej krótkiej odpowiedzi <b>nie skracamy</b>: <span class='bad'>Yes, I'm. ✗</span> <span class='good'>Yes, I am. ✓</span>" },
    { icon: "🔎", title: "Pytania o osobę", text: "<i>What's your name? Where are you from? How old are you? What's your nationality? How do you spell it? Who is he? What colour is it?</i>" },
    { icon: "⚠️", title: "Pułapka z języka polskiego", text: "Wiek po angielsku to <b>be</b>, nie <i>have</i>: <span class='bad'>I have 11 years. ✗</span> <span class='good'>I'm eleven. / I'm 11 years old. ✓</span>" }
  ];

  const MISTAKES = [
    { bad: "He are my friend.", good: "He is my friend.", why: "he → is" },
    { bad: "I is eleven.", good: "I am eleven.", why: "I → am" },
    { bad: "They is students.", good: "They are students.", why: "they → are" },
    { bad: "I amn't ten.", good: "I'm not ten.", why: "Nie mówimy amn't. I am not = I'm not." },
    { bad: "Yes, I'm.", good: "Yes, I am.", why: "W krótkiej odpowiedzi twierdzącej nie skracamy." },
    { bad: "Is she Polish? — Yes, she's.", good: "Is she Polish? — Yes, she is.", why: "Po Yes piszemy pełną formę." },
    { bad: "She aren't at school.", good: "She isn't at school.", why: "she → isn't" },
    { bad: "Are he a teacher?", good: "Is he a teacher?", why: "he → Is" },
    { bad: "Where you are from?", good: "Where are you from?", why: "W pytaniu be stoi przed osobą." },
    { bad: "I have eleven years.", good: "I'm eleven.", why: "Po angielsku wiek wyraża czasownik be." },
    { bad: "Are they at the park? — Yes, they aren't.", good: "Are they at the park? — Yes, they are.", why: "Yes łączy się z twierdzeniem, No z przeczeniem." },
    { bad: "You is not Polish.", good: "You aren't Polish.", why: "you → are not → aren't" }
  ];

  const BOOK_QUESTIONS = [
    { q: "What's your name?", pl: "Jak się nazywasz?", a: "My name's Lena. / I'm Lena." },
    { q: "Where are you from?", pl: "Skąd jesteś?", a: "I'm from Poland." },
    { q: "How old are you?", pl: "Ile masz lat?", a: "I'm ten. / I'm eleven." },
    { q: "What's your nationality?", pl: "Jakiej jesteś narodowości?", a: "I'm Polish." },
    { q: "How do you spell Martin?", pl: "Jak przeliterujesz Martin?", a: "M-A-R-T-I-N." },
    { q: "Who is he?", pl: "Kim on jest?", a: "He's my dad." },
    { q: "What colour is it?", pl: "Jakiego to jest koloru?", a: "It's blue." },
    { q: "Is Robbie a robot?", pl: "Czy Robbie jest robotem?", a: "Yes, he is." }
  ];

  const WORDS = {
    countries: [
      ["Poland", "Polska", "Polish"], ["Brazil", "Brazylia", "Brazilian"],
      ["the UK", "Wielka Brytania", "British"], ["the USA", "USA", "American"],
      ["France", "Francja", "French"], ["Spain", "Hiszpania", "Spanish"],
      ["Turkey", "Turcja", "Turkish"], ["China", "Chiny", "Chinese"],
      ["Australia", "Australia", "Australian"], ["Argentina", "Argentyna", "Argentinian"]
    ],
    adjectives: [
      ["amazing", "niesamowity"], ["cool", "fajny"], ["big", "duży"], ["small", "mały"],
      ["old", "stary"], ["new", "nowy"], ["interesting", "ciekawy"], ["funny", "zabawny"],
      ["clever", "bystry"], ["kind", "miły"], ["helpful", "pomocny"], ["shy", "nieśmiały"],
      ["friendly", "przyjazny"], ["quiet", "cichy"], ["sporty", "sportowy"], ["smart", "elegancki/bystry"],
      ["great", "świetny"], ["fantastic", "fantastyczny"], ["ready", "gotowy"], ["happy", "szczęśliwy"]
    ],
    places: [
      ["at school", "w szkole"], ["at home", "w domu"], ["in the park", "w parku"],
      ["in the garden", "w ogrodzie"], ["at a party", "na imprezie"], ["on holiday", "na wakacjach"],
      ["at a summer camp", "na obozie"], ["at a café", "w kawiarni"], ["at the film studio", "w studiu filmowym"]
    ]
  };

  /* ---------- 1. Wybierz formę (MCQ) ---------- */
  const choice = [
    { s: "I ___ eleven.", o: ["am", "is", "are"], a: "am", w: "I → am" },
    { s: "Mickey ___ from the UK.", o: ["am", "is", "are"], a: "is", w: "Mickey = he → is" },
    { s: "Ana and Mickey ___ friends.", o: ["am", "is", "are"], a: "are", w: "Ana and Mickey = they → are" },
    { s: "Robbie ___ a robot.", o: ["am", "is", "are"], a: "is", w: "Robbie = he → is" },
    { s: "We ___ students.", o: ["am", "is", "are"], a: "are", w: "we → are" },
    { s: "The notebooks ___ new!", o: ["am", "is", "are"], a: "are", w: "notebooks = they → are" },
    { s: "Lily ___ our cat.", o: ["am", "is", "are"], a: "is", w: "Lily = she/it → is" },
    { s: "You ___ amazing, Robbie!", o: ["am", "is", "are"], a: "are", w: "you → are" },
    { s: "Mr and Mrs Brown ___ cool.", o: ["am", "is", "are"], a: "are", w: "dwoje ludzi = they → are" },
    { s: "It ___ an orange book.", o: ["am", "is", "are"], a: "is", w: "it → is" },
    { s: "My name ___ Saskia.", o: ["am", "is", "are"], a: "is", w: "my name = it → is" },
    { s: "They ___ from Australia.", o: ["am", "is", "are"], a: "are", w: "they → are" },
    { s: "___ you from Brazil?", o: ["Am", "Is", "Are"], a: "Are", w: "you → Are you ...?" },
    { s: "___ she a new student?", o: ["Am", "Is", "Are"], a: "Is", w: "she → Is she ...?" },
    { s: "___ Robbie a child?", o: ["Am", "Is", "Are"], a: "Is", w: "Robbie = he → Is" },
    { s: "___ they at school today?", o: ["Am", "Is", "Are"], a: "Are", w: "they → Are" },
    { s: "The children ___ at school.", o: ["isn't", "aren't", "am not"], a: "aren't", w: "they → are not → aren't" },
    { s: "Mickey ___ from the USA.", o: ["isn't", "aren't", "am not"], a: "isn't", w: "he → is not → isn't" },
    { s: "I ___ at home today.", o: ["isn't", "aren't", "am not"], a: "am not", w: "I am not → I'm not / am not" },
    { s: "My hoodie ___ new.", o: ["isn't", "aren't", "am not"], a: "isn't", w: "it → isn't" },
    { s: "Lily and Robbie ___ robots.", o: ["isn't", "aren't", "am not"], a: "aren't", w: "they → aren't" },
    { s: "Where ___ you from?", o: ["am", "is", "are"], a: "are", w: "Where are you from?" },
    { s: "How old ___ Emile?", o: ["am", "is", "are"], a: "is", w: "Emile = he → is" },
    { s: "What colour ___ your jeans?", o: ["am", "is", "are"], a: "are", w: "jeans (liczba mnoga) → are" },
    { s: "She ___ Brazilian, she's from Spain.", o: ["is", "isn't", "are"], a: "isn't", w: "Zdanie zaprzecza: she isn't." },
    { s: "___ I right?", o: ["Am", "Is", "Are"], a: "Am", w: "I → Am I ...?" }
  ];

  /* ---------- 2. Wpisz formę (luk do wpisania) ---------- */
  const gap = [
    { s: "I ___ from Poland.", a: ["am", "'m"], h: "ja → ...", w: "I am from Poland." },
    { s: "He ___ my dad, Alex.", a: ["is", "'s"], h: "on → ...", w: "He is my dad." },
    { s: "We ___ from Manchester.", a: ["are", "'re"], h: "my → ...", w: "We are from Manchester." },
    { s: "They ___ cool.", a: ["are", "'re"], h: "oni → ...", w: "They are cool." },
    { s: "It ___ our homework!", a: ["is", "'s"], h: "to → ...", w: "It is our homework!" },
    { s: "Ana ___ ten.", a: ["is", "'s"], h: "ona → ...", w: "Ana is ten." },
    { s: "You ___ a robot, Robbie.", a: ["are", "'re"], h: "ty → ...", w: "You are a robot." },
    { s: "___ you a student?", a: ["are"], h: "Pytanie: be na początku.", w: "Are you a student?" },
    { s: "___ he from Canada?", a: ["is"], h: "he → ...", w: "Is he from Canada?" },
    { s: "___ Ana and Mickey at school today?", a: ["are"], h: "dwie osoby → ...", w: "Are Ana and Mickey at school today?" },
    { s: "Mickey ___ from the USA. He's from the UK.", a: ["isn't", "is not", "'s not"], h: "Przeczenie: is + not.", w: "Mickey isn't from the USA." },
    { s: "The children ___ at school today.", a: ["aren't", "are not", "'re not"], h: "Przeczenie: are + not.", w: "The children aren't at school today." },
    { s: "I ___ an actor.", a: ["'m not", "am not"], h: "Uwaga: nie mówimy amn't!", w: "I'm not an actor." },
    { s: "We ___ Chinese. We're from Poland.", a: ["aren't", "are not", "'re not"], h: "we → are not", w: "We aren't Chinese." },
    { s: "It ___ the right bag.", a: ["isn't", "is not", "'s not"], h: "it → is not", w: "It isn't the right bag." },
    { s: "Lily ___ at home.", a: ["isn't", "is not", "'s not"], h: "Lily = she → is not", w: "Lily isn't at home." },
    { s: "My shoes ___ new.", a: ["aren't", "are not", "'re not"], h: "shoes = they → are not", w: "My shoes aren't new." },
    { s: "___ they from China?", a: ["are"], h: "they → Are", w: "Are they from China?" },
    { s: "___ I in your class?", a: ["am"], h: "I → Am", w: "Am I in your class?" },
    { s: "What ___ your name?", a: ["is", "'s"], h: "What is = What's", w: "What is your name? / What's your name?" }
  ];

  /* ---------- 3/4. Pary (memory + łączenie) ---------- */
  const pairSets = [
    {
      name: "Zaimki ↔ formy be",
      pairs: [
        ["I", "am"], ["You", "are"], ["He", "is"], ["She", "is"],
        ["It", "is"], ["We", "are"], ["They", "are"], ["You (wy)", "are"]
      ]
    },
    {
      name: "Długa ↔ krótka forma",
      pairs: [
        ["I am", "I'm"], ["You are", "You're"], ["He is", "He's"], ["She is", "She's"],
        ["It is", "It's"], ["We are", "We're"], ["They are", "They're"], ["I am not", "I'm not"]
      ]
    },
    {
      name: "Przeczenia",
      pairs: [
        ["is not", "isn't"], ["are not", "aren't"], ["I am not", "I'm not"], ["He is not", "He isn't"],
        ["She is not", "She isn't"], ["We are not", "We aren't"], ["They are not", "They aren't"], ["It is not", "It isn't"]
      ]
    }
  ];

  const matchSets = [
    {
      name: "Pytanie ↔ krótka odpowiedź",
      pairs: [
        ["Are you from Poland?", "Yes, I am."],
        ["Is she a student?", "No, she isn't."],
        ["Are they at the party?", "Yes, they are."],
        ["Is he your brother?", "Yes, he is."],
        ["Are we late?", "No, we aren't."],
        ["Is it a school bag?", "No, it isn't."]
      ]
    },
    {
      name: "Pytanie ↔ odpowiedź z podręcznika",
      pairs: [
        ["How old are you?", "I'm ten."],
        ["What's your name?", "My name's Lena."],
        ["Where are you from?", "I'm from Poland."],
        ["What's your nationality?", "I'm Brazilian."],
        ["How do you spell Nathan?", "N-A-T-H-A-N."],
        ["Who is he?", "He's my dad."]
      ]
    },
    {
      name: "Twierdzenie ↔ przeczenie",
      pairs: [
        ["She is ready.", "She isn't ready."],
        ["They are at school.", "They aren't at school."],
        ["I am at home.", "I'm not at home."],
        ["He is a teacher.", "He isn't a teacher."],
        ["We are from Brazil.", "We aren't from Brazil."],
        ["It is a new bag.", "It isn't a new bag."]
      ]
    }
  ];

  /* ---------- 5. Rozsypanka (układanie wyrazów) ---------- */
  const order = [
    { words: ["is", "Polish.", "He"], answer: "He is Polish.", w: "Najpierw osoba, potem be." },
    { words: ["from", "are", "Brazil.", "We"], answer: "We are from Brazil.", w: "We are from Brazil." },
    { words: ["a", "robot.", "Robbie", "is"], answer: "Robbie is a robot.", w: "Robbie is a robot." },
    { words: ["students.", "They", "are"], answer: "They are students.", w: "They are students." },
    { words: ["at", "school.", "isn't", "She"], answer: "She isn't at school.", w: "Przeczenie: isn't po osobie." },
    { words: ["not", "at", "home.", "I'm"], answer: "I'm not at home.", w: "I'm not at home." },
    { words: ["new.", "My", "shoes", "aren't"], answer: "My shoes aren't new.", w: "My shoes aren't new." },
    { words: ["from", "you", "Are", "the UK?"], answer: "Are you from the UK?", w: "W pytaniu be jest pierwsze." },
    { words: ["she", "Is", "a", "student?"], answer: "Is she a student?", w: "Is she a student?" },
    { words: ["they", "Are", "at", "the park?"], answer: "Are they at the park?", w: "Are they at the park?" },
    { words: ["you", "are", "Where", "from?"], answer: "Where are you from?", w: "Where + be + osoba." },
    { words: ["old", "How", "are", "you?"], answer: "How old are you?", w: "How old are you?" },
    { words: ["your", "What's", "nationality?"], answer: "What's your nationality?", w: "What's your nationality?" },
    { words: ["Yes,", "I", "am."], answer: "Yes, I am.", w: "W krótkiej odpowiedzi NIE skracamy: Yes, I am." },
    { words: ["he", "No,", "isn't."], answer: "No, he isn't.", w: "No, he isn't." },
    { words: ["It's", "an", "amazing", "film."], answer: "It's an amazing film.", w: "It's an amazing film." },
    { words: ["book", ".", "a", "is", "It", "an"], answer: "It is a book.", extra: "an", w: "Jeden wyraz jest niepotrzebny! It is a book." },
    { words: ["are", "They", "students", ".", "a"], answer: "They are students.", extra: "a", w: "Przed liczbą mnogą nie dajemy a/an." }
  ];

  /* ---------- 6. Przekształcenia ---------- */
  const transform = [
    { from: "She is from Brazil.", task: "przeczenie", a: ["she isn't from brazil", "she is not from brazil", "she's not from brazil"], show: "She isn't from Brazil.", w: "is not → isn't" },
    { from: "They are at school.", task: "przeczenie", a: ["they aren't at school", "they are not at school", "they're not at school"], show: "They aren't at school.", w: "are not → aren't" },
    { from: "I am ten.", task: "przeczenie", a: ["i'm not ten", "i am not ten"], show: "I'm not ten.", w: "I am not → I'm not (nie: amn't!)" },
    { from: "He is a teacher.", task: "pytanie", a: ["is he a teacher?", "is he a teacher"], show: "Is he a teacher?", w: "be na początek + znak zapytania." },
    { from: "You are Polish.", task: "pytanie", a: ["are you polish?", "are you polish"], show: "Are you Polish?", w: "Are you Polish?" },
    { from: "Robbie is a robot.", task: "pytanie", a: ["is robbie a robot?", "is robbie a robot"], show: "Is Robbie a robot?", w: "Is Robbie a robot?" },
    { from: "We are from the UK.", task: "pytanie", a: ["are we from the uk?", "are we from the uk"], show: "Are we from the UK?", w: "Are we from the UK?" },
    { from: "It is a blue backpack.", task: "pytanie", a: ["is it a blue backpack?", "is it a blue backpack"], show: "Is it a blue backpack?", w: "Is it a blue backpack?" },
    { from: "Mickey is eleven.", task: "przeczenie", a: ["mickey isn't eleven", "mickey is not eleven", "mickey's not eleven"], show: "Mickey isn't eleven.", w: "Mickey = he → isn't" },
    { from: "Lily and Ana are friends.", task: "przeczenie", a: ["lily and ana aren't friends", "lily and ana are not friends", "lily and ana're not friends"], show: "Lily and Ana aren't friends.", w: "they → aren't" },
    { from: "Ms Collins is French.", task: "pytanie", a: ["is ms collins french?", "is ms collins french"], show: "Is Ms Collins French?", w: "Is Ms Collins French?" },
    { from: "The notebooks are orange.", task: "przeczenie", a: ["the notebooks aren't orange", "the notebooks are not orange", "the notebooks're not orange"], show: "The notebooks aren't orange.", w: "they → aren't" },
    { from: "I'm ready.", task: "pytanie", a: ["am i ready?", "am i ready"], show: "Am I ready?", w: "I → Am I ...?" },
    { from: "It's an old book.", task: "przeczenie", a: ["it isn't an old book", "it is not an old book", "it's not an old book"], show: "It isn't an old book.", w: "It's not / It isn't." },
    { from: "They're amazing.", task: "pytanie", a: ["are they amazing?", "are they amazing"], show: "Are they amazing?", w: "Are they amazing?" },
    { from: "He's at a summer camp.", task: "przeczenie", a: ["he isn't at a summer camp", "he is not at a summer camp", "he's not at a summer camp"], show: "He isn't at a summer camp.", w: "he → isn't" }
  ];

  /* ---------- 7. Krótkie odpowiedzi ---------- */
  const shorts = [
    { q: "Are you a student?", cue: "Tak", o: ["Yes, I am.", "Yes, I'm.", "Yes, I is.", "Yes, you are."], a: "Yes, I am.", w: "Po Yes nie skracamy: Yes, I am. (nie: Yes, I'm.)" },
    { q: "Are you from the USA, Mickey?", cue: "Nie", o: ["No, I'm not.", "No, I amn't.", "No, I isn't.", "No, you aren't."], a: "No, I'm not.", w: "Are you ...? → No, I'm not." },
    { q: "Is she a new student?", cue: "Nie", o: ["No, she isn't.", "No, she aren't.", "No, he isn't.", "No, she is."], a: "No, she isn't.", w: "she → isn't" },
    { q: "Is Robbie a child?", cue: "Nie", o: ["No, he isn't.", "No, she isn't.", "No, it isn't.", "No, he aren't."], a: "No, he isn't.", w: "Robbie = he. Jest robotem!" },
    { q: "Are they at the party?", cue: "Tak", o: ["Yes, they are.", "Yes, they're.", "Yes, we are.", "Yes, they is."], a: "Yes, they are.", w: "Po Yes pełna forma: they are." },
    { q: "Are Ana and Mickey Spanish?", cue: "Nie", o: ["No, they aren't.", "No, she isn't.", "No, we aren't.", "No, they isn't."], a: "No, they aren't.", w: "they → aren't" },
    { q: "Is it an orange book?", cue: "Tak", o: ["Yes, it is.", "Yes, it's.", "Yes, they are.", "Yes, it isn't."], a: "Yes, it is.", w: "Uwaga: nie Yes, it's!" },
    { q: "Is he your dad?", cue: "Tak", o: ["Yes, he is.", "Yes, he's.", "Yes, she is.", "Yes, I am."], a: "Yes, he is.", w: "he → is" },
    { q: "Are we late?", cue: "Nie", o: ["No, you aren't.", "No, we aren't.", "No, I'm not.", "No, you isn't."], a: "No, you aren't.", w: "Pytasz 'my?', ktoś odpowiada 'wy': No, you aren't. (W książce często też: No, we aren't.)" },
    { q: "Am I right?", cue: "Nie", o: ["No, you aren't.", "No, I'm not.", "No, you isn't.", "No, amn't."], a: "No, you aren't.", w: "Am I ...? → odpowiada druga osoba: No, you aren't." },
    { q: "Is your school bag blue?", cue: "Tak", o: ["Yes, it is.", "Yes, he is.", "Yes, they are.", "Yes, it's."], a: "Yes, it is.", w: "school bag = it" },
    { q: "Are your jeans new?", cue: "Nie", o: ["No, they aren't.", "No, it isn't.", "No, they isn't.", "No, we aren't."], a: "No, they aren't.", w: "jeans = liczba mnoga → they" },
    { q: "Is Lena from Poland?", cue: "Tak", o: ["Yes, she is.", "Yes, he is.", "Yes, she's.", "Yes, it is."], a: "Yes, she is.", w: "Lena = she" },
    { q: "Are you ten, Harry?", cue: "Tak", o: ["Yes, I am.", "Yes, I'm.", "Yes, we are.", "Yes, he is."], a: "Yes, I am.", w: "Harry: Really? Me too! — I'm ten." }
  ];

  /* ---------- 8. Detektyw błędów ---------- */
  const detective = [
    { words: ["He", "are", "my", "friend."], bad: 1, o: ["He is my friend.", "He am my friend.", "He are my friend."], a: "He is my friend.", w: "He → is." },
    { words: ["I", "is", "eleven."], bad: 1, o: ["I am eleven.", "I are eleven.", "I is eleven."], a: "I am eleven.", w: "I → am." },
    { words: ["They", "is", "students."], bad: 1, o: ["They are students.", "They is students.", "They am students."], a: "They are students.", w: "They → are." },
    { words: ["I", "amn't", "ten."], bad: 1, o: ["I'm not ten.", "I amn't ten.", "I isn't ten."], a: "I'm not ten.", w: "Nie ma słowa amn't! Mówimy I'm not." },
    { words: ["Yes,", "I'm."], bad: 1, o: ["Yes, I am.", "Yes, I'm.", "Yes, I is."], a: "Yes, I am.", w: "W krótkiej odpowiedzi twierdzącej nie skracamy." },
    { words: ["Are", "he", "a", "teacher?"], bad: 0, o: ["Is he a teacher?", "Are he a teacher?", "Am he a teacher?"], a: "Is he a teacher?", w: "he → Is he ...?" },
    { words: ["Where", "you", "are", "from?"], bad: 2, o: ["Where are you from?", "Where you are from?", "Where is you from?"], a: "Where are you from?", w: "be przed osobą: Where are you from?" },
    { words: ["She", "aren't", "at", "school."], bad: 1, o: ["She isn't at school.", "She aren't at school.", "She am not at school."], a: "She isn't at school.", w: "She → isn't." },
    { words: ["I", "have", "eleven", "years."], bad: 1, o: ["I'm eleven.", "I have eleven years.", "I is eleven."], a: "I'm eleven.", w: "Wiek po angielsku wyraża be: I'm eleven." },
    { words: ["Are", "they", "at", "the", "park?", "—", "Yes,", "they", "aren't."], bad: 8, o: ["Yes, they are.", "Yes, they aren't.", "No, they are."], a: "Yes, they are.", w: "Yes, they are. / No, they aren't." },
    { words: ["You", "is", "not", "Polish."], bad: 1, o: ["You aren't Polish.", "You is not Polish.", "You isn't Polish."], a: "You aren't Polish.", w: "You are not → You aren't." },
    { words: ["Is", "she", "Polish?", "—", "Yes,", "she's."], bad: 5, o: ["Yes, she is.", "Yes, she's.", "Yes, she are."], a: "Yes, she is.", w: "Po Yes piszemy pełną formę: Yes, she is." }
  ];

  /* ---------- 9. Sortownik zdań ---------- */
  const sort = [
    { s: "I am from Poland.", t: " affirmative" },
    { s: "She isn't at school.", t: "negative" },
    { s: "Are you from Brazil?", t: "question" },
    { s: "They're amazing!", t: "affirmative" },
    { s: "Is he a teacher?", t: "question" },
    { s: "We aren't from China.", t: "negative" },
    { s: "Robbie is a robot.", t: "affirmative" },
    { s: "It isn't a blue bag.", t: "negative" },
    { s: "Where are you from?", t: "question" },
    { s: "My shoes aren't new.", t: "negative" },
    { s: "Am I in your class?", t: "question" },
    { s: "You're amazing, Robbie!", t: "affirmative" },
    { s: "No, he isn't.", t: "negative" },
    { s: "What's your nationality?", t: "question" },
    { s: "Mickey and Ana are friends.", t: "affirmative" },
    { s: "I'm not at home today.", t: "negative" }
  ].map(function (x) { return { s: x.s, t: x.t.trim() }; });

  /* ---------- 10. Czytanie: Prawda / Fałsz ---------- */
  const reading = [
    {
      title: "Hello! I'm Mickey.",
      text: "Hello! I'm Mickey. I'm 11 years old and I'm British. She's my mum, Caroline, and he's my dad, Alex. They're cool. Look! It's our cat, Lily. Ana's here, too. She's 10 and she's from Brazil. Sorry! He's Robbie! He's a robot. You're amazing, Robbie!",
      items: [
        { s: "Mickey is from the UK.", t: true, w: "I'm British. / We're from Manchester in the UK." },
        { s: "Ana is eleven.", t: false, w: "She's 10, nie 11." },
        { s: "Lily and Robbie are robots.", t: false, w: "Lily to kot, tylko Robbie jest robotem." },
        { s: "Robbie is a robot.", t: true, w: "He's a robot!" },
        { s: "Ana isn't Brazilian.", t: false, w: "She's from Brazil → jest Brazylijką." }
      ]
    },
    {
      title: "Tomek's blog",
      text: "Hello everyone! My name is Tomek. I'm from Kraków in Poland. I'm fourteen years old. I'm with my brother, Filip. He is eleven years old. We are at a summer camp in the Tatra Mountains. The Tatra National Park is amazing. It's my favourite place. My brother loves it, too.",
      items: [
        { s: "Tomek is Polish.", t: true, w: "I'm from Kraków in Poland." },
        { s: "Tomek is 13 years old.", t: false, w: "I'm fourteen years old." },
        { s: "Filip is 11 years old.", t: true, w: "He is eleven years old." },
        { s: "Tomek and Filip are at home.", t: false, w: "We are at a summer camp." },
        { s: "The park isn't amazing.", t: false, w: "The Tatra National Park is amazing." }
      ]
    },
    {
      title: "A new student",
      text: "Hi! My name's Emile Martin. I'm a new student. I'm French and I'm from France. Ms Collins is my English teacher. She isn't from the UK — she's from Canada. My friends aren't at school today. They're at a party. It's OK!",
      items: [
        { s: "Emile is French.", t: true, w: "I'm French." },
        { s: "Ms Collins is British.", t: false, w: "She isn't from the UK — she's from Canada." },
        { s: "Emile's friends are at school.", t: false, w: "They're at a party." },
        { s: "Emile isn't a new student.", t: false, w: "I'm a new student." },
        { s: "Ms Collins is from Canada.", t: true, w: "she's from Canada." }
      ]
    }
  ];

  /* ---------- 11. Słuchanie / dyktando ---------- */
  const listening = [
    { spoken: "I'm eleven.", o: ["I'm eleven.", "I'm seven.", "Are you eleven?"], a: "I'm eleven." },
    { spoken: "She isn't at school.", o: ["She isn't at school.", "She is at school.", "Is she at school?"], a: "She isn't at school." },
    { spoken: "Are they from China?", o: ["Are they from China?", "They're from China.", "Are we from China?"], a: "Are they from China?" },
    { spoken: "We're from Brazil.", o: ["We're from Brazil.", "We aren't from Brazil.", "Are we from Brazil?"], a: "We're from Brazil." },
    { spoken: "It's an orange book.", o: ["It's an orange book.", "It isn't an orange book.", "Is it an orange book?"], a: "It's an orange book." },
    { spoken: "He's a robot.", o: ["He's a robot.", "He isn't a robot.", "Is he a robot?"], a: "He's a robot." },
    { spoken: "My shoes aren't new.", o: ["My shoes aren't new.", "My shoes are new.", "Are my shoes new?"], a: "My shoes aren't new." },
    { spoken: "Is she a student?", o: ["Is she a student?", "She is a student.", "She isn't a student."], a: "Is she a student?" },
    { spoken: "You're amazing, Robbie!", o: ["You're amazing, Robbie!", "Are you amazing, Robbie?", "You aren't amazing, Robbie!"], a: "You're amazing, Robbie!" },
    { spoken: "They aren't at the party.", o: ["They aren't at the party.", "They're at the party.", "Are they at the party?"], a: "They aren't at the party." },
    { spoken: "Where are you from?", o: ["Where are you from?", "Where is he from?", "Are you from Poland?"], a: "Where are you from?" },
    { spoken: "No, he isn't.", o: ["No, he isn't.", "No, she isn't.", "Yes, he is."], a: "No, he isn't." }
  ];

  const dictation = [
    { spoken: "isn't", hint: "is + not, krótko (6 liter z apostrofem)", a: ["isn't", "isnt"] },
    { spoken: "aren't", hint: "are + not, krótko", a: ["aren't", "arent"] },
    { spoken: "I'm", hint: "ja jestem — krótka forma", a: ["i'm", "im"] },
    { spoken: "he's", hint: "on jest — krótka forma", a: ["he's", "hes"] },
    { spoken: "she's", hint: "ona jest — krótka forma", a: ["she's", "shes"] },
    { spoken: "we're", hint: "my jesteśmy — krótka forma", a: ["we're", "were"] },
    { spoken: "they're", hint: "oni są — krótka forma", a: ["they're", "theyre"] },
    { spoken: "it's", hint: "to jest — krótka forma", a: ["it's", "its"] },
    { spoken: "you're", hint: "ty jesteś — krótka forma", a: ["you're", "youre"] },
    { spoken: "I'm not", hint: "ja nie jestem", a: ["i'm not", "im not", "i am not"] },
    { spoken: "Yes, I am.", hint: "krótka odpowiedź twierdząca", a: ["yes, i am", "yes i am"] },
    { spoken: "Are you ready?", hint: "pytanie o gotowość", a: ["are you ready", "are you ready?"] }
  ];

  /* ---------- 12. Tłumaczenie z polskiego ---------- */
  const translate = [
    { pl: "Jestem z Polski.", words: ["I'm", "from", "Poland.", "is", "am"], answer: "I'm from Poland." },
    { pl: "Ona nie jest z Brazylii.", words: ["She", "isn't", "from", "Brazil.", "is", "are"], answer: "She isn't from Brazil." },
    { pl: "Czy oni są w szkole?", words: ["Are", "they", "at", "school?", "is", "They"], answer: "Are they at school?" },
    { pl: "On jest robotem.", words: ["He's", "a", "robot.", "are", "is"], answer: "He's a robot." },
    { pl: "Nie jestem w domu.", words: ["I'm", "not", "at", "home.", "am", "isn't"], answer: "I'm not at home." },
    { pl: "Skąd jesteś?", words: ["Where", "are", "you", "from?", "is", "do"], answer: "Where are you from?" },
    { pl: "Moje buty nie są nowe.", words: ["My", "shoes", "aren't", "new.", "isn't", "are"], answer: "My shoes aren't new." },
    { pl: "Tak, jestem.", words: ["Yes,", "I", "am.", "I'm.", "is"], answer: "Yes, I am." },
    { pl: "Czy ona jest nową uczennicą?", words: ["Is", "she", "a", "new", "student?", "Are", "he"], answer: "Is she a new student?" },
    { pl: "Nie, nie jesteśmy.", words: ["No,", "we", "aren't.", "are", "isn't"], answer: "No, we aren't." },
    { pl: "Ile masz lat?", words: ["How", "old", "are", "you?", "is", "What"], answer: "How old are you?" },
    { pl: "To jest niesamowity film.", words: ["It's", "an", "amazing", "film.", "a", "is"], answer: "It's an amazing film." },
    { pl: "Mickey i Ana nie są Hiszpanami.", words: ["Mickey", "and", "Ana", "aren't", "Spanish.", "isn't"], answer: "Mickey and Ana aren't Spanish." },
    { pl: "Czy jesteś gotowy?", words: ["Are", "you", "ready?", "Is", "Do"], answer: "Are you ready?" }
  ];

  /* ---------- 13. Literowanie (wisielec) ---------- */
  const spelling = [
    { word: "isn't", pl: "nie jest (on/ona/to)" },
    { word: "aren't", pl: "nie są / nie jesteśmy" },
    { word: "I'm", pl: "ja jestem" },
    { word: "he's", pl: "on jest" },
    { word: "she's", pl: "ona jest" },
    { word: "it's", pl: "to jest" },
    { word: "we're", pl: "my jesteśmy" },
    { word: "you're", pl: "ty jesteś / wy jesteście" },
    { word: "they're", pl: "oni są" },
    { word: "I'm not", pl: "ja nie jestem" },
    { word: "Yes, I am.", pl: "Tak, jestem." },
    { word: "Are you...?", pl: "Czy ty jesteś...?" }
  ];

  /* ---------- Fiszki ---------- */
  const flashcards = [
    { front: "I", back: "am • I'm • I'm not • Am I ...?", tag: "odmiana" },
    { front: "he / she / it", back: "is • he's • isn't • Is he ...?", tag: "odmiana" },
    { front: "you / we / they", back: "are • you're • aren't • Are you ...?", tag: "odmiana" },
    { front: "is not =", back: "isn't", tag: "przeczenia" },
    { front: "are not =", back: "aren't", tag: "przeczenia" },
    { front: "I am not =", back: "I'm not (NIE: amn't)", tag: "przeczenia" },
    { front: "Jak zrobić pytanie?", back: "be na początek: She is Polish. → Is she Polish?", tag: "pytania" },
    { front: "Are you ten?", back: "Yes, I am. / No, I'm not.", tag: "odpowiedzi" },
    { front: "Is she a student?", back: "Yes, she is. / No, she isn't.", tag: "odpowiedzi" },
    { front: "Yes, I'm.", back: "BŁĄD! Poprawnie: Yes, I am.", tag: "pułapki" },
    { front: "I have 11 years.", back: "BŁĄD! Poprawnie: I'm eleven.", tag: "pułapki" },
    { front: "Skąd jesteś?", back: "Where are you from?", tag: "pytania" },
    { front: "Ile masz lat?", back: "How old are you?", tag: "pytania" },
    { front: "Jakiej jesteś narodowości?", back: "What's your nationality?", tag: "pytania" },
    { front: "Jak się nazywasz?", back: "What's your name?", tag: "pytania" },
    { front: "Where you are from?", back: "BŁĄD! Poprawnie: Where are you from?", tag: "pułapki" }
  ];

  /* ---------- Kartkówka próbna (ujednolicony format) ---------- */
  const examPool = [
    { type: "choice", p: "I ___ eleven.", o: ["am", "is", "are"], a: "am", w: "I → am", area: "odmiana" },
    { type: "choice", p: "Mickey and Ana ___ friends.", o: ["am", "is", "are"], a: "are", w: "they → are", area: "odmiana" },
    { type: "choice", p: "Robbie ___ a robot, not a cat.", o: ["am", "is", "are"], a: "is", w: "he → is", area: "odmiana" },
    { type: "choice", p: "The notebooks ___ orange now!", o: ["am", "is", "are"], a: "are", w: "they → are", area: "odmiana" },
    { type: "choice", p: "___ you from the UK?", o: ["Am", "Is", "Are"], a: "Are", w: "you → Are", area: "pytania" },
    { type: "choice", p: "___ she at school today?", o: ["Am", "Is", "Are"], a: "Is", w: "she → Is", area: "pytania" },
    { type: "choice", p: "Where ___ you from?", o: ["am", "is", "are"], a: "are", w: "Where are you from?", area: "pytania" },
    { type: "choice", p: "How old ___ Filip?", o: ["am", "is", "are"], a: "is", w: "he → is", area: "pytania" },
    { type: "choice", p: "We ___ from China. We're from Poland.", o: ["isn't", "aren't", "am not"], a: "aren't", w: "we → aren't", area: "przeczenia" },
    { type: "choice", p: "Lily ___ at home.", o: ["isn't", "aren't", "am not"], a: "isn't", w: "she → isn't", area: "przeczenia" },
    { type: "choice", p: "I ___ at home today.", o: ["isn't", "aren't", "am not"], a: "am not", w: "I'm not / am not", area: "przeczenia" },
    { type: "choice", p: "My shoes ___ new.", o: ["isn't", "aren't", "am not"], a: "aren't", w: "shoes = they", area: "przeczenia" },
    { type: "choice", p: "Are you a student? — ___", o: ["Yes, I am.", "Yes, I'm.", "Yes, I is."], a: "Yes, I am.", w: "Po Yes nie skracamy.", area: "odpowiedzi" },
    { type: "choice", p: "Is Robbie a child? — ___", o: ["No, he isn't.", "No, she isn't.", "No, he aren't."], a: "No, he isn't.", w: "Robbie = he, jest robotem.", area: "odpowiedzi" },
    { type: "choice", p: "Are they at the park? — Yes, ___", o: ["they are", "they're", "they is"], a: "they are", w: "Krótka odpowiedź: pełna forma.", area: "odpowiedzi" },
    { type: "choice", p: "Is it a blue bag? — No, ___", o: ["it isn't", "it aren't", "it am not"], a: "it isn't", w: "it → isn't", area: "odpowiedzi" },
    { type: "choice", p: "Które zdanie jest poprawne?", o: ["He is my friend.", "He are my friend.", "He am my friend."], a: "He is my friend.", w: "he → is", area: "pułapki" },
    { type: "choice", p: "Które zdanie jest poprawne?", o: ["I'm eleven.", "I have eleven years.", "I is eleven."], a: "I'm eleven.", w: "Wiek = be.", area: "pułapki" },
    { type: "choice", p: "Które zdanie jest poprawne?", o: ["Is he a teacher?", "Are he a teacher?", "Does he be a teacher?"], a: "Is he a teacher?", w: "be na początku, bez do/does.", area: "pułapki" },
    { type: "choice", p: "Które zdanie jest poprawne?", o: ["Where are you from?", "Where you are from?", "Where is you from?"], a: "Where are you from?", w: "Where + be + osoba.", area: "pułapki" },
    { type: "choice", p: "Które zdanie jest PRZECZENIEM?", o: ["They aren't students.", "Are they students?", "They're students."], a: "They aren't students.", w: "aren't = przeczenie.", area: "rozpoznawanie" },
    { type: "choice", p: "Które zdanie jest PYTANIEM?", o: ["Is she Polish?", "She is Polish.", "She isn't Polish."], a: "Is she Polish?", w: "be przed osobą + ?.", area: "rozpoznawanie" },
    { type: "input", p: "Uzupełnij: She ___ my mum, Caroline.", a: ["is", "'s"], w: "She is my mum.", area: "odmiana" },
    { type: "input", p: "Uzupełnij: They ___ from Australia.", a: ["are", "'re"], w: "They are from Australia.", area: "odmiana" },
    { type: "input", p: "Uzupełnij: ___ you OK?", a: ["are"], w: "Are you OK?", area: "pytania" },
    { type: "input", p: "Uzupełnij: ___ he from Canada?", a: ["is"], w: "Is he from Canada?", area: "pytania" },
    { type: "input", p: "Uzupełnij PRZECZENIE: Mickey ___ from the USA.", a: ["isn't", "is not", "'s not"], w: "Mickey isn't from the USA.", area: "przeczenia" },
    { type: "input", p: "Uzupełnij PRZECZENIE: The children ___ at school.", a: ["aren't", "are not", "'re not"], w: "The children aren't at school.", area: "przeczenia" },
    { type: "input", p: "Uzupełnij PRZECZENIE: I ___ an actor.", a: ["'m not", "am not"], w: "I'm not an actor. (nie: amn't!)", area: "przeczenia" },
    { type: "input", p: "Napisz PYTANIE: He is a teacher. → ___", a: ["is he a teacher?", "is he a teacher"], w: "Is he a teacher?", area: "pytania" },
    { type: "input", p: "Napisz PYTANIE: You are Polish. → ___", a: ["are you polish?", "are you polish"], w: "Are you Polish?", area: "pytania" },
    { type: "input", p: "Napisz PRZECZENIE: They are ready. → ___", a: ["they aren't ready", "they are not ready", "they're not ready"], w: "They aren't ready.", area: "przeczenia" },
    { type: "input", p: "Napisz PRZECZENIE: She is from Brazil. → ___", a: ["she isn't from brazil", "she is not from brazil", "she's not from brazil"], w: "She isn't from Brazil.", area: "przeczenia" },
    { type: "input", p: "Odpowiedz twierdząco: Are you ten? → ___", a: ["yes, i am", "yes i am"], w: "Yes, I am. (nie: Yes, I'm.)", area: "odpowiedzi" },
    { type: "input", p: "Odpowiedz przecząco: Is she a student? → ___", a: ["no, she isn't", "no she isn't", "no, she is not"], w: "No, she isn't.", area: "odpowiedzi" },
    { type: "input", p: "Odpowiedz twierdząco: Are they friends? → ___", a: ["yes, they are", "yes they are"], w: "Yes, they are.", area: "odpowiedzi" },
    { type: "input", p: "Popraw zdanie: He are my friend. → ___", a: ["he is my friend", "he is my friend.", "he's my friend"], w: "He is my friend.", area: "pułapki" },
    { type: "input", p: "Popraw zdanie: Where you are from? → ___", a: ["where are you from?", "where are you from"], w: "Where are you from?", area: "pułapki" },
    { type: "input", p: "Po angielsku: „Ona nie jest w szkole.” → ___", a: ["she isn't at school", "she is not at school", "she's not at school"], w: "She isn't at school.", area: "tłumaczenie" },
    { type: "input", p: "Po angielsku: „Czy oni są z Chin?” → ___", a: ["are they from china?", "are they from china"], w: "Are they from China?", area: "tłumaczenie" },
    { type: "input", p: "Po angielsku: „Nie jestem gotowy.” → ___", a: ["i'm not ready", "i am not ready"], w: "I'm not ready.", area: "tłumaczenie" },
    { type: "input", p: "Po angielsku: „Jestem z Polski.” → ___", a: ["i'm from poland", "i am from poland"], w: "I'm from Poland.", area: "tłumaczenie" }
  ];

  /* ---------- Plan nauki do kartkówki ---------- */
  const plan = [
    { day: "Dzień 1", title: "Odmiana i twierdzenia", todo: "Tabela BE, Wybierz formę, Wpisz formę, Memory" },
    { day: "Dzień 2", title: "Przeczenia", todo: "Przekształcenia, Detektyw błędów, Literowanie (isn't / aren't)" },
    { day: "Dzień 3", title: "Pytania i krótkie odpowiedzi", todo: "Krótkie odpowiedzi, Rozsypanka, Połącz w pary, Sortownik" },
    { day: "Dzień 4", title: "Wszystko naraz + słuchanie", todo: "Słuchaj i wybierz, Dyktando, Tłumaczenie, Prawda/Fałsz, Szybka runda" },
    { day: "Dzień 5", title: "Próbna kartkówka", todo: "Kartkówka próbna ×2, Fiszki, popraw tylko to, co sprawiło trudność" }
  ];

  const ACTIVITIES = [
    { id: "choice", icon: "🎯", title: "Wybierz formę", desc: "am / is / are, przeczenia i pytania — szybkie pytania z podpowiedzią", skill: "odmiana" },
    { id: "gap", icon: "✏️", title: "Wpisz formę", desc: "Wpisz brakującą formę be. Sprawdzamy też krótkie formy!", skill: "odmiana" },
    { id: "memory", icon: "🃏", title: "Memory", desc: "Odkrywaj karty i łącz zaimki z formami oraz formy długie z krótkimi", skill: "odmiana" },
    { id: "match", icon: "🔗", title: "Połącz w pary", desc: "Pytanie ↔ odpowiedź, twierdzenie ↔ przeczenie", skill: "odpowiedzi" },
    { id: "order", icon: "🧱", title: "Rozsypanka", desc: "Ułóż wyrazy w zdaniu. Uwaga na jeden niepotrzebny wyraz!", skill: "pytania" },
    { id: "transform", icon: "🔄", title: "Przekształcenia", desc: "Twierdzenie → przeczenie albo pytanie. Napisz sam!", skill: "przeczenia" },
    { id: "shorts", icon: "💬", title: "Krótkie odpowiedzi", desc: "Yes, I am. / No, he isn't. — wybierz właściwą reakcję", skill: "odpowiedzi" },
    { id: "detective", icon: "🕵️", title: "Detektyw błędów", desc: "Znajdź błędne słowo w zdaniu i napraw je", skill: "pułapki" },
    { id: "sort", icon: "🗂️", title: "Sortownik zdań", desc: "Wrzuć zdanie do koszyka: twierdzenie / pytanie / przeczenie", skill: "rozpoznawanie" },
    { id: "reading", icon: "📖", title: "Prawda czy fałsz?", desc: "Krótkie teksty o postaciach z podręcznika — T czy F?", skill: "czytanie" },
    { id: "listen", icon: "🎧", title: "Słuchaj i wybierz", desc: "Robbie czyta zdania po angielsku, a Ty wybierasz, co słyszysz", skill: "słuchanie" },
    { id: "dictation", icon: "🔤", title: "Dyktando", desc: "Usłysz formę i wpisz ją — trenuj apostrof w isn't / aren't / I'm", skill: "pisownia" },
    { id: "translate", icon: "🇵🇱", title: "Tłumacz z polskiego", desc: "Zbuduj angielskie zdanie z klocków (z pułapkami)", skill: "tłumaczenie" },
    { id: "spelling", icon: "🎲", title: "Literuj formę", desc: "Odgaduj litery krótkich form, zanim Robbie straci antenkę", skill: "pisownia" },
    { id: "mix", icon: "🧩", title: "Miks 5 minut", desc: "12 zadań z 5 różnych typów w jednym zestawie — przeplatany trening (interleaving)", skill: "powtórka" },
    { id: "speed", icon: "⚡", title: "Szybka runda", desc: "60 sekund, ile poprawnych odpowiedzi zdążysz?", skill: "powtórka" }
  ];

  /* =======================================================================
     MEGA-BAZA ZADAŃ — kilka tysięcy unikalnych, poprawnych językowo zadań.
     Generator jest DETERMINISTYCZNY (stałe ziarno), więc każde zadanie ma
     stały identyfikator: system „bez powtórek" pamięta je między wizytami,
     a plik HTML pozostaje mały (zadania powstają przy otwarciu strony).
     Wszystkie zdania dotyczą wyłącznie czasownika „to be" i używają słów
     z podręcznika Together 2026, klasa 4.
     ======================================================================= */
  (function megaBank() {
    "use strict";
    let seed = 20261110;
    function rnd() {
      seed = (seed + 0x6D2B79F5) | 0;
      let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    }
    function sh(a) {
      const x = a.slice();
      for (let i = x.length - 1; i > 0; i--) { const j = Math.floor(rnd() * (i + 1)); const t = x[i]; x[i] = x[j]; x[j] = t; }
      return x;
    }
    function cap(s) { return s.charAt(0).toUpperCase() + s.slice(1); }
    function low1(s) { return s.charAt(0).toLowerCase() + s.slice(1); }
    function nrm(s) {
      return String(s).toLowerCase().replace(/[’`´]/g, "'").replace(/\s+/g, " ").replace(/[.!?]+$/, "").trim();
    }
    function uniq(a) { const o = {}, r = []; a.forEach(function (x) { if (!o[x]) { o[x] = 1; r.push(x); } }); return r; }

    const SFX = { am: "'m", is: "'s", are: "'re" };
    const NEGW = { am: "am not", is: "isn't", are: "aren't" };
    const ANS = {
      I: ["Yes, I am.", "No, I'm not."], you: ["Yes, you are.", "No, you aren't."],
      we: ["Yes, we are.", "No, we aren't."], he: ["Yes, he is.", "No, he isn't."],
      she: ["Yes, she is.", "No, she isn't."], it: ["Yes, it is.", "No, it isn't."],
      they: ["Yes, they are.", "No, they aren't."]
    };
    function plVerb(be, low) {
      if (be === "am") return ["jestem", "nie jestem"];
      if (be === "is") return ["jest", "nie jest"];
      if (low === "ty") return ["jesteś", "nie jesteś"];
      if (low === "my") return ["jesteśmy", "nie jesteśmy"];
      return ["są", "nie są"];
    }
    /* s — podmiot, be — forma, ans — zaimek w krótkiej odpowiedzi,
       pl/low — podmiot po polsku, proper — imię (zostawiamy wielką literę) */
    function mk(s, be, ans, pl, low, proper, noShort) {
      const v = plVerb(be, low), PRON = ["I", "You", "He", "She", "It", "We", "They"];
      const qt = PRON.indexOf(s) !== -1 ? (s === "I" ? "I" : s.toLowerCase()) : (proper ? s : low1(s));
      const THINGS = ["It", "Lily", "The robot", "My cat", "My bag", "The film", "My book",
                      "My school bag", "My hoodie", "My shoes", "My jeans", "The notebooks", "My books"];
      return {
        s: s, be: be, qt: qt, ans: ans, pl: pl, low: low,
        person: THINGS.indexOf(s) === -1,
        short: noShort ? null : s + SFX[be],
        neg: be === "am" ? "I'm not" : s + " " + NEGW[be],
        negL: s + " " + be + " not", negW: NEGW[be],
        q: cap(be) + " " + qt, yes: ANS[ans][0], no: ANS[ans][1],
        plBe: v[0], plNeg: v[1]
      };
    }
    const SUBS = [
      mk("I", "am", "you", "Ja", "ja", false),
      mk("You", "are", "I", "Ty", "ty", false),
      mk("He", "is", "he", "On", "on", false),
      mk("She", "is", "she", "Ona", "ona", false),
      mk("It", "is", "it", "To", "to", false),
      mk("We", "are", "you", "My", "my", false),
      mk("They", "are", "they", "Oni", "oni", false),
      mk("Mickey", "is", "he", "Mickey", "Mickey", true),
      mk("Ana", "is", "she", "Ana", "Ana", true),
      mk("Robbie", "is", "he", "Robbie", "Robbie", true),
      mk("Lily", "is", "it", "Lily", "Lily", true),
      mk("Emile", "is", "he", "Emile", "Emile", true),
      mk("Lena", "is", "she", "Lena", "Lena", true),
      mk("Tomek", "is", "he", "Tomek", "Tomek", true),
      mk("Filip", "is", "he", "Filip", "Filip", true),
      mk("Harry", "is", "he", "Harry", "Harry", true),
      mk("Saskia", "is", "she", "Saskia", "Saskia", true),
      mk("Ms Collins", "is", "she", "Pani Collins", "pani Collins", true),
      mk("Mr Brown", "is", "he", "Pan Brown", "pan Brown", true),
      mk("My dad", "is", "he", "Mój tata", "mój tata", false),
      mk("My mum", "is", "she", "Moja mama", "moja mama", false),
      mk("My friend", "is", "he", "Mój kolega", "mój kolega", false),
      mk("My sister", "is", "she", "Moja siostra", "moja siostra", false),
      mk("My teacher", "is", "she", "Moja nauczycielka", "moja nauczycielka", false),
      mk("The robot", "is", "it", "Robot", "robot", false),
      mk("My cat", "is", "it", "Mój kot", "mój kot", false),
      mk("My bag", "is", "it", "Moja torba", "moja torba", false),
      mk("The film", "is", "it", "Film", "film", false),
      mk("My book", "is", "it", "Moja książka", "moja książka", false),
      mk("My school bag", "is", "it", "Mój plecak", "mój plecak", false),
      mk("My hoodie", "is", "it", "Moja bluza", "moja bluza", false),
      mk("Mickey and Ana", "are", "they", "Mickey i Ana", "Mickey i Ana", true, true),
      mk("Lily and Robbie", "are", "they", "Lily i Robbie", "Lily i Robbie", true, true),
      mk("My friends", "are", "they", "Moi przyjaciele", "moi przyjaciele", false, true),
      mk("My parents", "are", "they", "Moi rodzice", "moi rodzice", false, true),
      mk("My shoes", "are", "they", "Moje buty", "moje buty", false, true),
      mk("My jeans", "are", "they", "Moje dżinsy", "moje dżinsy", false, true),
      mk("The children", "are", "they", "Dzieci", "dzieci", false, true),
      mk("The students", "are", "they", "Uczniowie", "uczniowie", false, true),
      mk("The notebooks", "are", "they", "Zeszyty", "zeszyty", false, true),
      mk("My books", "are", "they", "Moje książki", "moje książki", false, true)
    ];

    const ADJ = [
      ["amazing", "niesamowity"], ["cool", "fajny"], ["big", "duży"], ["small", "mały"],
      ["old", "stary"], ["new", "nowy"], ["interesting", "ciekawy"], ["funny", "zabawny"],
      ["clever", "bystry"], ["kind", "miły"], ["helpful", "pomocny"], ["shy", "nieśmiały"],
      ["friendly", "przyjazny"], ["quiet", "cichy"], ["sporty", "sportowy"], ["smart", "bystry"],
      ["great", "świetny"], ["fantastic", "fantastyczny"], ["ready", "gotowy"], ["happy", "szczęśliwy"],
      ["tired", "zmęczony"], ["busy", "zajęty"], ["late", "spóźniony"], ["nice", "miły"],
      ["brave", "odważny"], ["young", "młody"], ["strong", "silny"], ["fast", "szybki"],
      ["sad", "smutny"], ["lucky", "szczęśliwy"]
    ];
    const NAT = ["Polish", "British", "American", "French", "Spanish", "Turkish", "Chinese", "Brazilian", "Australian", "Argentinian", "Canadian"];
    const FROM = [
      ["from Poland", "z Polski"], ["from Brazil", "z Brazylii"], ["from the UK", "z Wielkiej Brytanii"],
      ["from the USA", "z USA"], ["from France", "z Francji"], ["from Spain", "z Hiszpanii"],
      ["from Turkey", "z Turcji"], ["from China", "z Chin"], ["from Australia", "z Australii"],
      ["from Argentina", "z Argentyny"], ["from Canada", "z Kanady"], ["from Manchester", "z Manchesteru"],
      ["from Kraków", "z Krakowa"]
    ];
    const PLACE = [
      ["at school", "w szkole"], ["at home", "w domu"], ["in the park", "w parku"],
      ["in the garden", "w ogrodzie"], ["at a party", "na imprezie"], ["on holiday", "na wakacjach"],
      ["at a summer camp", "na obozie"], ["at a café", "w kawiarni"], ["in the classroom", "w klasie"],
      ["at the cinema", "w kinie"], ["in the library", "w bibliotece"], ["on the bus", "w autobusie"],
      ["at the beach", "na plaży"], ["in the kitchen", "w kuchni"], ["at the zoo", "w zoo"]
    ];
    const NOUN = ["a student", "a teacher", "a robot", "a cat", "a friend", "a new student",
                  "an actor", "an amazing film", "an old book", "an orange bag", "a blue hoodie", "a good answer"];
    const AGE = ["eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen"];
    const COLOUR = ["blue", "red", "green", "orange", "yellow", "black", "white", "pink", "purple", "brown"];
    const COMP = [];
    ADJ.forEach(function (a) { COMP.push({ en: a[0], pl: null, kind: "adj" }); });
    NAT.forEach(function (a) { COMP.push({ en: a, pl: null, kind: "nat" }); });
    FROM.forEach(function (a) { COMP.push({ en: a[0], pl: a[1], kind: "from" }); });
    PLACE.forEach(function (a) { COMP.push({ en: a[0], pl: a[1], kind: "place" }); });
    NOUN.forEach(function (a) { COMP.push({ en: a, pl: null, kind: "noun" }); });
    AGE.forEach(function (a) { COMP.push({ en: a, pl: null, kind: "age" }); });

    function aff(x, c) { return x.s + " " + x.be + " " + c.en + "."; }
    function affS(x, c) { return x.short ? x.short + " " + c.en + "." : aff(x, c); }
    function negS(x, c) { return x.neg + " " + c.en + "."; }
    function negL(x, c) { return x.negL + " " + c.en + "."; }
    function que(x, c) { return x.q + " " + c.en + "?"; }
    function where(x) { return "Where " + x.be + " " + x.qt + " from?"; }
    function howOld(x) { return "How old " + x.be + " " + x.qt + "?"; }

    const DISTRACT = ["am", "is", "are", "isn't", "aren't", "am not", "do", "does", "not",
                      "a", "an", "Yes,", "No,", "Where", "How", "What", "the", "at", "in", "on", "and"];
    function mkTiles(sentence, n, wantExtra) {
      const base = sentence.trim().split(/\s+/), seen = {}, used = {};
      for (let i = 0; i < base.length; i++) { const k = base[i].toLowerCase(); if (seen[k]) return null; seen[k] = true; used[k] = true; }
      const words = base.slice(), dist = sh(DISTRACT.filter(function (d) { return !used[d.toLowerCase()]; }));
      let added = 0, extra = null;
      for (let i = 0; i < dist.length && added < n; i++) {
        const k = dist[i].toLowerCase();
        if (seen[k]) continue;
        seen[k] = true; words.push(dist[i]);
        if (wantExtra && extra === null) extra = dist[i];
        added++;
      }
      return { words: sh(words), extra: wantExtra ? extra : null };
    }

    const ALL = [];
    SUBS.forEach(function (x) { COMP.forEach(function (c) {
      if (c.kind === "noun" && x.be === "are") return;      // „They are a student” ✗
      if (c.kind === "age" && !x.person) return;            // „The film is eleven” ✗
      ALL.push({ x: x, c: c });
    }); });
    function take(n) { return sh(ALL).slice(0, n); }
    const KEYS = {};
    function unique(bank, str) { const k = bank + "|" + str.toLowerCase(); if (KEYS[k]) return false; KEYS[k] = true; return true; }

    /* ---- 1. Wybierz formę — 4 warianty ---- */
    take(520).forEach(function (o, k) {
      const x = o.x, c = o.c, v = k % 4;
      if (v === 0) choice.push({ s: x.s + " ___ " + c.en + ".", o: ["am", "is", "are"], a: x.be, w: x.s + " → " + x.be, area: "odmiana" });
      else if (v === 1) choice.push({ s: "___ " + x.qt + " " + c.en + "?", o: ["Am", "Is", "Are"], a: cap(x.be), w: "Pytanie zaczyna się od " + cap(x.be) + ".", area: "pytania" });
      else if (v === 2) choice.push({ s: x.s + " ___ not " + c.en + ".", o: ["am", "is", "are"], a: x.be, w: x.be + " not = " + x.negW, area: "przeczenia" });
      else choice.push({ s: "Uzupełnij przeczenie: " + x.s + " ___ " + c.en + ".", o: ["isn't", "aren't", "am not"], a: x.negW, w: x.s + " → " + x.neg, area: "przeczenia" });
    });

    /* ---- 2. Wpisz formę ---- */
    take(440).forEach(function (o, k) {
      const x = o.x, c = o.c, v = k % 3;
      if (v === 0) gap.push({ s: x.s + " ___ " + c.en + ".", a: uniq(x.short ? [x.be, SFX[x.be]] : [x.be]), h: x.s + " → " + x.be + ". Krótko: " + (x.short || "—"), w: aff(x, c), area: "odmiana" });
      else if (v === 1) gap.push({ s: "___ " + x.qt + " " + c.en + "?", a: uniq([x.be, cap(x.be)]), h: "W pytaniu be stoi na początku.", w: que(x, c), area: "pytania" });
      else gap.push({ s: "Uzupełnij przeczenie: " + x.s + " ___ " + c.en + ".", a: uniq([x.negW, x.be + " not", x.short ? SFX[x.be] + " not" : x.be + " not"]), h: "Dodaj not po be → " + x.negW + ".", w: negS(x, c), area: "przeczenia" });
    });

    /* ---- 5. Rozsypanka ---- */
    take(300).forEach(function (o, k) {
      const x = o.x, c = o.c, v = k % 4;
      const sen = v === 0 ? aff(x, c) : v === 1 ? negS(x, c) : v === 2 ? que(x, c)
                : (x.person ? (k % 8 === 0 ? where(x) : howOld(x)) : where(x));
      if (!unique("order", sen)) return;
      const t = mkTiles(sen, 2 + (k % 2), k % 4 === 3);
      if (!t) return;
      const it = { words: t.words, answer: sen, w: v === 2 || v === 3 ? "W pytaniu be jest przed osobą." : "Najpierw osoba, potem be.", area: v === 1 ? "przeczenia" : v === 2 || v === 3 ? "pytania" : "odmiana" };
      if (t.extra) it.extra = t.extra;
      order.push(it);
    });

    /* ---- 6. Przekształcenia ---- */
    take(360).forEach(function (o, k) {
      const x = o.x, c = o.c;
      if (k % 2 === 0) {
        const from = aff(x, c), show = negS(x, c);
        const acc = [nrm(show), nrm(negL(x, c))];
        if (x.short) acc.push(nrm(x.short + " not " + c.en + "."));
        transform.push({ from: from, task: "przeczenie", a: uniq(acc), show: show, w: x.be + " + not → " + x.negW, area: "przeczenia" });
      } else {
        const from = aff(x, c), show = que(x, c);
        transform.push({ from: from, task: "pytanie", a: uniq([nrm(show)]), show: show, w: "be na początek: " + x.q + " ...?", area: "pytania" });
      }
    });

    /* ---- 7. Krótkie odpowiedzi ---- */
    const TRAPS = ["Yes, I'm.", "No, I amn't.", "Yes, he's.", "Yes, they're.", "No, she are.", "Yes, it's.", "No, he aren't.", "Yes, we're."];
    take(300).forEach(function (o, k) {
      const x = o.x, c = o.c, tak = k % 2 === 0, good = tak ? x.yes : x.no;
      const q = x.q + " " + c.en + "?";
      const wrongs = [
        tak ? x.no : x.yes,
        ANS[sh(["I", "you", "we", "he", "she", "it", "they"])[0]][tak ? 0 : 1],
        sh(TRAPS)[0]
      ];
      const opts = uniq([good].concat(wrongs));
      ["Yes, I am.", "No, I'm not.", "Yes, he is.", "No, she isn't.", "Yes, they are.", "No, we aren't.", "Yes, it is.", "No, he isn't."].forEach(function (z) { if (opts.length < 4) opts.push(z); });
      shorts.push({ q: q, cue: tak ? "Tak" : "Nie", o: uniq(opts).slice(0, 4), a: good, w: tak ? "Po Yes piszemy pełną formę: " + good : "Przeczenie: " + good, area: "odpowiedzi" });
    });

    /* ---- 8. Detektyw błędów ---- */
    take(240).forEach(function (o, k) {
      const x = o.x, c = o.c;
      const good = aff(x, c);
      const wrongBe = sh(["am", "is", "are"].filter(function (b) { return b !== x.be; }))[0];
      const bad = x.s + " " + wrongBe + " " + c.en + ".";
      const bad2 = x.s + " " + sh(["am", "is", "are"].filter(function (b) { return b !== x.be && b !== wrongBe; }))[0] + " " + c.en + ".";
      const words = bad.split(/\s+/);
      const bi = words.indexOf(wrongBe);
      if (bi === -1) return;
      detective.push({ words: words, bad: bi, o: uniq([good, bad, bad2]).slice(0, 3), a: good, w: x.s + " → " + x.be + ", nie " + wrongBe + ".", area: "pułapki" });
      if (detective[detective.length - 1].o.length !== 3) detective.pop();
    });

    /* ---- 9. Sortownik ---- */
    take(420).forEach(function (o, k) {
      const x = o.x, c = o.c, v = k % 3;
      sort.push({ s: v === 0 ? aff(x, c) : v === 1 ? negS(x, c) : que(x, c), t: v === 0 ? "affirmative" : v === 1 ? "negative" : "question", area: v === 1 ? "przeczenia" : v === 2 ? "pytania" : "odmiana" });
    });

    /* ---- 11. Słuchanie ---- */
    take(300).forEach(function (o, k) {
      const x = o.x, c = o.c, v = k % 3;
      const spoken = v === 0 ? aff(x, c) : v === 1 ? negS(x, c) : que(x, c);
      const opts = uniq([spoken, v === 1 ? aff(x, c) : negS(x, c), v === 2 ? aff(x, c) : que(x, c)]);
      while (opts.length < 3) opts.push(affS(x, c));
      listening.push({ spoken: spoken, o: uniq(opts).slice(0, 3), a: spoken, area: v === 1 ? "przeczenia" : v === 2 ? "pytania" : "odmiana" });
    });

    /* ---- 12. Dyktando ---- */
    SUBS.forEach(function (x) {
      if (x.short) dictation.push({ spoken: x.short, hint: "krótka forma: " + x.s + " " + x.be, a: uniq([nrm(x.short), nrm(x.s + " " + x.be)]) });
      dictation.push({ spoken: x.neg, hint: "przeczenie: " + x.s + " " + x.be + " not", a: uniq([nrm(x.neg), nrm(x.negL)]) });
      dictation.push({ spoken: x.yes, hint: "krótka odpowiedź twierdząca", a: uniq([nrm(x.yes)]) });
      dictation.push({ spoken: x.no, hint: "krótka odpowiedź przecząca", a: uniq([nrm(x.no)]) });
    });
    NAT.concat(COLOUR, AGE, ADJ.map(function (a) { return a[0]; })).forEach(function (w) {
      dictation.push({ spoken: w, hint: "słowo z rozdziału 1–2 · " + w.length + " liter", a: uniq([nrm(w)]) });
    });

    /* ---- 13. Tłumaczenie z polskiego ---- */
    take(260).forEach(function (o, k) {
      const x = o.x, c = o.c;
      if (!c.pl) return;
      const v = k % 3;
      let pl, sen, w;
      if (v === 0) { pl = x.pl + " " + x.plBe + " " + c.pl + "."; sen = aff(x, c); w = x.s + " → " + x.be; }
      else if (v === 1) { pl = x.pl + " " + x.plNeg + " " + c.pl + "."; sen = negS(x, c); w = "Przeczenie: " + x.neg; }
      else { pl = "Czy " + x.low + " " + x.plBe + " " + c.pl + "?"; sen = que(x, c); w = "be na początku: " + x.q; }
      if (!unique("translate", sen)) return;
      const t = mkTiles(sen, 2, false);
      if (!t) return;
      translate.push({ pl: pl, words: t.words, answer: sen, w: w, area: "tłumaczenie" });
    });
    SUBS.filter(function (x) { return x.be === "is"; }).slice(0, 14).forEach(function (x, k) {
      const sen = x.s + " is " + AGE[k % AGE.length] + ".";
      const pl = x.pl + " ma " + ["osiem", "dziewięć", "dziesięć", "jedenaście", "dwanaście", "trzynaście", "czternaście"][k % AGE.length] + " lat.";
      const t = mkTiles(sen, 2, false);
      if (t && unique("translate", sen)) translate.push({ pl: pl, words: t.words, answer: sen, w: "Uwaga! Wiek po angielsku to BE, nie have: " + sen, area: "pułapki" });
    });

    /* ---- 14. Literowanie ---- */
    const SPELL = [];
    SUBS.forEach(function (x) {
      if (x.short) SPELL.push({ word: x.short, pl: x.s + " " + x.be + " — krótka forma" });
      SPELL.push({ word: x.negW, pl: x.s + " " + x.be + " not — przeczenie" });
    });
    NAT.concat(COLOUR, ["student", "teacher", "robot", "friend", "Poland", "Brazil", "France", "Spain", "China", "amazing", "clever", "friendly"]).forEach(function (w) {
      SPELL.push({ word: w, pl: "słowo z rozdziału 1–2" });
    });
    SPELL.forEach(function (s) { if (unique("spelling", s.word)) spelling.push(s); });

    /* ---- Fiszki ---- */
    take(70).forEach(function (o, k) {
      const x = o.x, c = o.c, v = k % 4;
      if (v === 0 && x.short) flashcards.push({ front: "Krótka forma: " + x.s + " " + x.be, back: x.short, tag: "odmiana" });
      else if (v === 1) flashcards.push({ front: "Przeczenie: " + aff(x, c), back: negS(x, c), tag: "przeczenia" });
      else if (v === 2) flashcards.push({ front: "Pytanie: " + aff(x, c), back: que(x, c), tag: "pytania" });
      else flashcards.push({ front: "Odpowiedz twierdząco: " + que(x, c), back: x.yes, tag: "odpowiedzi" });
    });

    /* ---- Pary (memory) i łączenie ---- */
    const SHORTABLE = SUBS.filter(function (x) { return x.short; });
    for (let k = 0; k < 24; k++) {
      const kind = k % 4, picked = sh(SUBS).slice(0, 8), picked2 = sh(SHORTABLE).slice(0, 8);
      let pairs, name;
      if (kind === 0) { name = "Osoba ↔ forma be · zestaw " + (k + 1); pairs = picked.map(function (x) { return [x.s, x.be]; }); }
      else if (kind === 1) { name = "Długa ↔ krótka forma · zestaw " + (k + 1); pairs = picked2.map(function (x) { return [x.s + " " + x.be, x.short]; }); }
      else if (kind === 2) { name = "Przeczenia · zestaw " + (k + 1); pairs = picked.map(function (x) { return [x.negL, x.neg]; }); }
      else { name = "Pytanie ↔ odpowiedź · zestaw " + (k + 1); pairs = picked.map(function (x) { return [x.q + " ...?", x.yes]; }); }
      if (pairs.length === 8) pairSets.push({ name: name, pairs: pairs });
    }
    for (let k = 0; k < 24; k++) {
      const kind = k % 4, picked = sh(SUBS).slice(0, 6), cc = sh(COMP.filter(function (c) { return c.kind === "place" || c.kind === "from"; }));
      let pairs, name;
      if (kind === 0) { name = "Pytanie ↔ krótka odpowiedź · zestaw " + (k + 1); pairs = picked.map(function (x, i) { return [x.q + " " + cc[i % cc.length].en + "?", k % 2 ? x.no : x.yes]; }); }
      else if (kind === 1) { name = "Twierdzenie ↔ przeczenie · zestaw " + (k + 1); pairs = picked.map(function (x, i) { return [aff(x, cc[i % cc.length]), negS(x, cc[i % cc.length])]; }); }
      else if (kind === 2) { name = "Angielski ↔ polski · zestaw " + (k + 1); pairs = cc.slice(0, 6).map(function (c) { return [c.en, c.pl]; }); }
      else { name = "Krótka ↔ długa forma · zestaw " + (k + 1); pairs = sh(SHORTABLE).slice(0, 6).map(function (x) { return [x.short, x.s + " " + x.be]; }); }
      if (pairs.length === 6) matchSets.push({ name: name, pairs: pairs });
    }

    /* ---- Czytanie: Prawda / Fałsz ---- */
    const NAMES = ["Mickey", "Ana", "Lena", "Tomek", "Emile", "Saskia", "Harry", "Nathan", "Filip", "Martin"];
    const FRIENDS = ["Robbie", "Lily", "Caroline", "Alex", "Ms Collins", "Nathan", "Ana", "Mickey"];
    const PLACES_EN = PLACE.map(function (p) { return p[0]; });
    for (let k = 0; k < 24; k++) {
      const name = NAMES[k % NAMES.length], friend = FRIENDS[k % FRIENDS.length];
      const age = AGE[k % AGE.length], age2 = AGE[(k + 3) % AGE.length];
      const nat = NAT[k % NAT.length], adj = ADJ[k % ADJ.length][0];
      const place = PLACES_EN[k % PLACES_EN.length];
      const col = COLOUR[k % COLOUR.length], col2 = COLOUR[(k + 2) % COLOUR.length];
      reading.push({
        title: "Hello! I'm " + name + ".",
        text: "Hi! I'm " + name + ". I'm " + age + " and I'm " + nat + ". " + friend +
              " is my friend. " + friend + " is " + adj + ". We are " + place + ". My bag is " + col + ". It isn't " + col2 + ".",
        items: [
          { s: name + " is " + nat + ".", t: true, w: "W tekście: I'm " + nat + "." },
          { s: name + " is " + age2 + ".", t: false, w: "W tekście: I'm " + age + ", nie " + age2 + "." },
          { s: friend + " is " + adj + ".", t: true, w: "W tekście: " + friend + " is " + adj + "." },
          { s: name + " and " + friend + " are " + place + ".", t: true, w: "W tekście: We are " + place + "." },
          { s: "My bag is " + col2 + ".", t: false, w: "W tekście: My bag is " + col + ". It isn't " + col2 + "." }
        ]
      });
    }

    /* ---- Kartkówka próbna: pula zadań ---- */
    sh(choice).slice(0, 800).forEach(function (it) {
      examPool.push({ type: "choice", p: it.s, o: it.o.slice(), a: it.a, w: it.w, area: it.area || "odmiana" });
    });
    sh(gap).slice(0, 700).forEach(function (it) {
      examPool.push({ type: "input", p: "Uzupełnij: " + it.s, a: it.a.slice(), w: it.w, area: it.area || "odmiana" });
    });

    /* ---- Usuwamy ewentualne duplikaty (pierwszeństwo mają zadania z podręcznika) ----
       Dzięki temu „nowy zestaw" nigdy nie zawiera tego samego zdania dwa razy. */
    function dedupe(arr, keyFn) {
      const seen = {}, out = [];
      arr.forEach(function (it) { const k = keyFn(it); if (!seen[k]) { seen[k] = true; out.push(it); } });
      arr.length = 0; out.forEach(function (it) { arr.push(it); });
    }
    dedupe(choice, function (x) { return x.s + "|" + x.a; });
    dedupe(gap, function (x) { return x.s + "|" + x.a[0]; });
    dedupe(order, function (x) { return x.answer; });
    dedupe(translate, function (x) { return x.pl + "|" + x.answer; });
    dedupe(transform, function (x) { return x.from + "|" + x.task; });
    dedupe(shorts, function (x) { return x.q + "|" + x.cue; });
    dedupe(detective, function (x) { return x.words.join(" ") + "|" + x.a; });
    dedupe(sort, function (x) { return x.s; });
    dedupe(listening, function (x) { return x.spoken; });
    dedupe(dictation, function (x) { return x.spoken; });
    dedupe(spelling, function (x) { return x.word; });
    dedupe(flashcards, function (x) { return x.front; });
    dedupe(reading, function (x) { return x.text; });
    dedupe(examPool, function (x) { return x.p + "|" + JSON.stringify(x.o || x.a); });
    dedupe(pairSets, function (x) { return x.name; });
    dedupe(matchSets, function (x) { return x.name; });

    /* ---- Stałe identyfikatory (klucz dla systemu bez powtórek) ---- */
    const BANKS = [
      [choice, "ch", "odmiana"], [gap, "gp", "odmiana"], [order, "or", "pytania"],
      [transform, "tr", "przeczenia"], [shorts, "sh", "odpowiedzi"], [detective, "dt", "pułapki"],
      [sort, "sr", "rozpoznawanie"], [listening, "li", "słuchanie"], [dictation, "dc", "pisownia"],
      [translate, "tl", "tłumaczenie"], [spelling, "sp", "pisownia"], [flashcards, "fc", "odmiana"],
      [examPool, "ex", "odmiana"]
    ];
    BANKS.forEach(function (b) {
      b[0].forEach(function (it, i) { if (!it.id) it.id = b[1] + i; if (!it.area) it.area = b[2]; });
    });
    pairSets.forEach(function (s, i) { if (!s.id) s.id = "ps" + i; });
    matchSets.forEach(function (s, i) { if (!s.id) s.id = "ms" + i; });
    reading.forEach(function (r, i) { if (!r.id) r.id = "rd" + i; r.items.forEach(function (it, j) { if (!it.id) it.id = "rd" + i + "-" + j; if (!it.area) it.area = "czytanie"; }); });
  })();

  const BANK_COUNT = choice.length + gap.length + order.length + transform.length + shorts.length +
    detective.length + sort.length + listening.length + dictation.length + translate.length +
    spelling.length + flashcards.length + examPool.length + reading.length * 5 +
    pairSets.length * 8 + matchSets.length * 6;

  root.TOBE_DATA = {
    meta: {
      subject: "Język angielski",
      type: "Kartkówka",
      date: "2026-11-10",
      scope: "czasownik 'to be' (być): odmiana przez osoby, twierdzenia, pytania i przeczenia",
      book: "Together 2026 · klasa 4 · Unit 1 (s. 10–11, 13–16) i Unit 2 (s. 18–21), zeszyt ćwiczeń, 'Przygotowanie do sprawdzianu'",
      bankTotal: BANK_COUNT,
      bank: {
        choice: choice.length, gap: gap.length, pairSets: pairSets.length, matchSets: matchSets.length,
        order: order.length, transform: transform.length, shorts: shorts.length, detective: detective.length,
        sort: sort.length, reading: reading.length, listening: listening.length, dictation: dictation.length,
        translate: translate.length, spelling: spelling.length, flashcards: flashcards.length, examPool: examPool.length
      }
    },
    be: BE, rules: RULES, mistakes: MISTAKES, bookQuestions: BOOK_QUESTIONS, words: WORDS,
    choice: choice, gap: gap, pairSets: pairSets, matchSets: matchSets, order: order,
    transform: transform, shorts: shorts, detective: detective, sort: sort, reading: reading,
    listening: listening, dictation: dictation, translate: translate, spelling: spelling,
    flashcards: flashcards, examPool: examPool, plan: plan, activities: ACTIVITIES,
    machine: MACHINE
  };
})(typeof window !== "undefined" ? window : globalThis);
