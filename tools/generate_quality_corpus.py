#!/usr/bin/env python3
"""Generate the reviewed EN/RU quality corpus without invoking tts-front."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "tests" / "quality" / "en_ru_sentences.tsv"
HEADER = "id\tlanguage\tmode\tcategory\texpectation\tinput\texpected"


def row(case_id, language, mode, category, expectation, source, expected):
    fields = (case_id, language, mode, category, expectation, source, expected)
    assert all("\t" not in field and "\n" not in field for field in fields)
    assert language in {"en", "ru"}
    assert mode in {"explicit", "auto_segment"}
    assert expectation in {"normalize", "preserve"}
    assert (source == expected) == (expectation == "preserve")
    return "\t".join(fields)


rows = []

en_numbers = {
    "0": "zero",
    "1": "one",
    "2": "two",
    "3": "three",
    "4": "four",
    "5": "five",
    "10": "ten",
    "11": "eleven",
    "12": "twelve",
    "20": "twenty",
    "21": "twenty one",
    "42": "forty two",
    "99": "ninety nine",
    "100": "one hundred",
    "250": "two hundred fifty",
}

ru_numbers = {
    "0": "ноль",
    "1": "один",
    "2": "два",
    "3": "три",
    "4": "четыре",
    "5": "пять",
    "10": "десять",
    "11": "одиннадцать",
    "12": "двенадцать",
    "20": "двадцать",
    "21": "двадцать один",
    "42": "сорок два",
    "99": "девяносто девять",
    "100": "сто",
    "250": "двести пятьдесят",
}

en_integer_templates = [
    ("reported", "The reported value is {n}.", "The reported value is {w}."),
    ("build", "Build {n} completed successfully.", "Build {w} completed successfully."),
    ("response", "The server returned code {n}.", "The server returned code {w}."),
    ("parentheses", "The retry threshold is ({n}).", "The retry threshold is ({w})."),
]
for digits, words in en_numbers.items():
    for name, source, expected in en_integer_templates:
        rows.append(row(f"en-int-{digits}-{name}", "en", "explicit", "integer", "normalize",
                        source.format(n=digits), expected.format(w=words)))

ru_integer_templates = [
    ("reported", "В отчёте указано число {n}.", "В отчёте указано число {w}."),
    ("build", "Сборка {n} завершилась успешно.", "Сборка {w} завершилась успешно."),
    ("response", "Сервер вернул код {n}.", "Сервер вернул код {w}."),
    ("parentheses", "Контрольное значение равно ({n}).", "Контрольное значение равно ({w})."),
]
for digits, words in ru_numbers.items():
    for name, source, expected in ru_integer_templates:
        rows.append(row(f"ru-int-{digits}-{name}", "ru", "explicit", "integer", "normalize",
                        source.format(n=digits), expected.format(w=words)))

en_measurements = [
    ("1 kg", "one kilogram", "The package weighs {}.", "Maximum package weight: {}."),
    ("2 kg", "two kilograms", "The package weighs {}.", "Maximum package weight: {}."),
    ("5 km", "five kilometers", "The route is {} long.", "Route length: {}."),
    ("10 m", "ten meters", "The cable is {} long.", "Cable length: {}."),
    ("21 cm", "twenty one centimeters", "The panel is {} wide.", "Panel width: {}."),
    ("42 mm", "forty two millimeters", "The bolt is {} long.", "Bolt length: {}."),
    ("1 GB", "one gigabyte", "The buffer size is {}.", "Buffer limit: {}."),
    ("2 GB", "two gigabytes", "The buffer size is {}.", "Buffer limit: {}."),
    ("1 MB", "one megabyte", "The attachment size is {}.", "Attachment limit: {}."),
    ("100 MB", "one hundred megabytes", "The attachment size is {}.", "Attachment limit: {}."),
]
for index, (surface, spoken, first, second) in enumerate(en_measurements):
    for variant, template in enumerate((first, second)):
        rows.append(row(f"en-unit-{index}-{variant}", "en", "explicit", "measurement",
                        "normalize", template.format(surface), template.format(spoken)))

ru_measurements = [
    ("1 кг", "один килограмм", "Посылка весит {}.", "Предельный вес посылки: {}."),
    ("2 кг", "два килограмма", "Посылка весит {}.", "Предельный вес посылки: {}."),
    ("5 кг", "пять килограммов", "Посылка весит {}.", "Предельный вес посылки: {}."),
    ("1 км", "один километр", "Маршрут имеет длину {}.", "Длина маршрута: {}."),
    ("2 км", "два километра", "Маршрут имеет длину {}.", "Длина маршрута: {}."),
    ("5 км", "пять километров", "Маршрут имеет длину {}.", "Длина маршрута: {}."),
    ("1 м", "один метр", "Кабель имеет длину {}.", "Длина кабеля: {}."),
    ("2 м", "два метра", "Кабель имеет длину {}.", "Длина кабеля: {}."),
    ("5 см", "пять сантиметров", "Панель имеет ширину {}.", "Ширина панели: {}."),
    ("21 мм", "двадцать один миллиметр", "Деталь имеет толщину {}.", "Толщина детали: {}."),
]
for index, (surface, spoken, first, second) in enumerate(ru_measurements):
    for variant, template in enumerate((first, second)):
        rows.append(row(f"ru-unit-{index}-{variant}", "ru", "explicit", "measurement",
                        "normalize", template.format(surface), template.format(spoken)))

en_currency = [
    ("$1", "one dollar"),
    ("$2", "two dollars"),
    ("$5", "five dollars"),
    ("$12.50", "twelve dollars fifty cents"),
    ("$1.01", "one dollar one cent"),
]
for index, (surface, spoken) in enumerate(en_currency):
    for variant, template in enumerate(("The service costs {}.", "Price ({}) includes tax.")):
        rows.append(row(f"en-money-{index}-{variant}", "en", "explicit", "currency",
                        "normalize", template.format(surface), template.format(spoken)))

ru_currency = [
    ("1 рубль", "один рубль"),
    ("2 рубля", "два рубля"),
    ("5 рублей", "пять рублей"),
    ("21 рубль", "двадцать один рубль"),
    ("42 рубля", "сорок два рубля"),
]
for index, (surface, spoken) in enumerate(ru_currency):
    for variant, template in enumerate(("Подписка стоит {}.", "Цена ({}) указана без скидки.")):
        rows.append(row(f"ru-money-{index}-{variant}", "ru", "explicit", "currency",
                        "normalize", template.format(surface), template.format(spoken)))

ru_percent_words = {
    "0": "ноль процентов",
    "1": "один процент",
    "2": "два процента",
    "3": "три процента",
    "4": "четыре процента",
    "5": "пять процентов",
    "10": "десять процентов",
    "11": "одиннадцать процентов",
    "12": "двенадцать процентов",
    "20": "двадцать процентов",
    "21": "двадцать один процент",
    "42": "сорок два процента",
    "99": "девяносто девять процентов",
    "100": "сто процентов",
    "250": "двести пятьдесят процентов",
}
for index, (digits, words) in enumerate(en_numbers.items()):
    rows.append(row(f"en-percent-{index}", "en", "explicit", "percent", "normalize",
                    f"The report shows {digits}%.", f"The report shows {words} percent."))
for index, (digits, spoken) in enumerate(ru_percent_words.items()):
    rows.append(row(f"ru-percent-{index}", "ru", "explicit", "percent", "normalize",
                    f"В отчёте указано: {digits}%.", f"В отчёте указано: {spoken}."))

decimal_cases = [
    ("en", "0.5", "zero point five"),
    ("en", "1.25", "one point two five"),
    ("en", "-2.5", "minus two point five"),
    ("en", "12.05", "twelve point zero five"),
    ("en", "-0.01", "minus zero point zero one"),
    ("en", "99.9", "ninety nine point nine"),
    ("ru", "0,5", "ноль целых пять десятых"),
    ("ru", "1,25", "одна целая двадцать пять сотых"),
    ("ru", "-2,5", "минус две целых пять десятых"),
    ("ru", "12,05", "двенадцать целых пять сотых"),
    ("ru", "-0,01", "минус ноль целых одна сотая"),
    ("ru", "99,9", "девяносто девять целых девять десятых"),
]
for index, (language, surface, spoken) in enumerate(decimal_cases):
    templates = (("The measured value is {}.", "Measured value: {}.")) if language == "en" else (
        "Измеренное значение: {}.", "Результат измерения — {}.")
    for variant, template in enumerate(templates):
        rows.append(row(f"{language}-decimal-{index}-{variant}", language, "explicit", "decimal",
                        "normalize", template.format(surface), template.format(spoken)))

punctuation_pairs = [
    ("en", "The values are 1, 2, and 3.", "The values are one, two, and three."),
    ("en", "Use (2) workers.", "Use (two) workers."),
    ("en", "She said “42”.", "She said “forty two”."),
    ("en", "Result: -5.", "Result: minus five."),
    ("en", "Versions 2 and 3 are supported.", "Versions two and three are supported."),
    ("ru", "Значения: 1, 2 и 3.", "Значения: один, два и три."),
    ("ru", "Используйте (2) процесса.", "Используйте (два) процесса."),
    ("ru", "Он сказал «42».", "Он сказал «сорок два»."),
    ("ru", "Результат: -5.", "Результат: минус пять."),
    ("ru", "Поддерживаются версии 2 и 3.", "Поддерживаются версии два и три."),
]
suffixes = {
    "en": ("", " Check complete.", " The value was confirmed."),
    "ru": ("", " Проверка завершена.", " Значение подтверждено."),
}
for index, (language, source, expected) in enumerate(punctuation_pairs):
    for repeat, suffix in enumerate(suffixes[language]):
        rows.append(row(f"{language}-punct-{index}-{repeat}", language, "explicit", "punctuation",
                        "normalize", source + suffix, expected + suffix))

time_cases = [
    ("en", "1:01", "one hour one minute"),
    ("en", "2:02", "two hours two minutes"),
    ("en", "6:06", "six hours six minutes"),
    ("en", "12:30", "twelve hours thirty minutes"),
    ("en", "21:21", "twenty one hours twenty one minutes"),
    ("en", "23:59", "twenty three hours fifty nine minutes"),
    ("ru", "1:01", "один час одна минута"),
    ("ru", "2:02", "два часа две минуты"),
    ("ru", "6:06", "шесть часов шесть минут"),
    ("ru", "12:30", "двенадцать часов тридцать минут"),
    ("ru", "21:21", "двадцать один час двадцать одна минута"),
    ("ru", "23:59", "двадцать три часа пятьдесят девять минут"),
]
for index, (language, surface, spoken) in enumerate(time_cases):
    template = "Timer reading: {}." if language == "en" else "Показание таймера: {}."
    rows.append(row(f"{language}-time-{index}", language, "explicit", "time", "normalize",
                    template.format(surface), template.format(spoken)))

ru_dates = [
    ("01.02.2026", "первое февраля две тысячи двадцать шестого года"),
    ("31.12.1987", "тридцать первое декабря тысяча девятьсот восемьдесят седьмого года"),
    ("01.02.2011", "первое февраля две тысячи одиннадцатого года"),
    ("01.02.2012", "первое февраля две тысячи двенадцатого года"),
    ("01.02.2019", "первое февраля две тысячи девятнадцатого года"),
    ("01.02.2020", "первое февраля две тысячи двадцатого года"),
    ("01.02.2042", "первое февраля две тысячи сорок второго года"),
    ("15.05.2025", "пятнадцатое мая две тысячи двадцать пятого года"),
]
for index, (surface, spoken) in enumerate(ru_dates):
    rows.append(row(f"ru-date-{index}", "ru", "explicit", "date", "normalize",
                    f"Дата встречи: {surface}.", f"Дата встречи: {spoken}."))

en_dates = [
    ("January 2, 2026", "January second, two thousand twenty six"),
    ("March 21, 2025", "March twenty first, two thousand twenty five"),
    ("December 31, 1999", "December thirty first, nineteen ninety nine"),
    ("May 1, 2020", "May first, two thousand twenty"),
]
for index, (surface, spoken) in enumerate(en_dates):
    rows.append(row(f"en-date-spoken-{index}", "en", "explicit", "date", "normalize",
                    f"The meeting is on {surface}.", f"The meeting is on {spoken}."))
for index, surface in enumerate(("01/02/2026", "02/01/2026", "2026-02-01", "2025-12-31")):
    source = f"Keep the machine-readable date {surface}."
    rows.append(row(f"en-date-preserve-{index}", "en", "explicit", "date", "preserve", source,
                    source))

mixed_cases = [
    ("ru", "У меня 2 GPU и 3 ядра. Использую OpenAI API.",
     "У меня два GPU и три ядра. Использую OpenAI API."),
    ("ru", "Видеокарта RTX 4090 работает на 1.5 GB RAM.",
     "Видеокарта RTX 4090 работает на one point five GB RAM."),
    ("ru", "HTTP/2 вернул 2 ошибки.", "HTTP/2 вернул две ошибки."),
    ("ru", "Сборка 7 использует C++17.", "Сборка семь использует C++17."),
    ("ru", "Напишите dev42@example.com после 3 попыток.",
     "Напишите dev42@example.com после трёх попыток."),
    ("en", "Use 2 кг for the calibration weight.",
     "Use два килограмма for the calibration weight."),
    ("en", "Build 7 targets RTX-4090.", "Build seven targets RTX-4090."),
    ("en", "HTTP/2 returned 3 errors.", "HTTP/2 returned three errors."),
    ("en", "Email dev42@example.com after 2 attempts.",
     "Email dev42@example.com after two attempts."),
    ("en", "The OpenAI API accepted 42 requests.",
     "The OpenAI API accepted forty two requests."),
]
for index, (language, source, expected) in enumerate(mixed_cases):
    rows.append(row(f"{language}-mixed-{index}", language, "auto_segment", "mixed_language",
                    "normalize", source, expected))

preserved = [
    ("en", "Deploy C++17 to RTX-4090 at https://example.com/v2.1.0."),
    ("en", "The endpoint HTTP/2 returned code #123."),
    ("en", "Keep malformed value 1.2..3 unchanged."),
    ("en", "Keep malformed time 1:02::3 unchanged."),
    ("en", "Identifier V2.1.0—123 must remain atomic."),
    ("en", "The malformed amount 1 dollarfoo stays unchanged."),
    ("en", "The malformed unit 2 kgfoo stays unchanged."),
    ("en", "Invalid clock values 24:00 and 99:99 remain digits."),
    ("ru", "Запустите C++17 на RTX-4090 через https://example.com/v2.1.0."),
    ("ru", "Ответ HTTP/2 содержит код #123."),
    ("ru", "Некорректное значение 1,2..3 сохраняется."),
    ("ru", "Некорректное время 1:02::3 сохраняется."),
    ("ru", "Идентификатор V2.1.0—123 остаётся атомарным."),
    ("ru", "Некорректная сумма 1 рублейfoo сохраняется."),
    ("ru", "Некорректная единица 2 кгfoo сохраняется."),
    ("ru", "Ошибочное время 24:00 и 99:99 остаётся цифрами."),
]
for index, (language, source) in enumerate(preserved):
    rows.append(row(f"{language}-preserve-{index}", language, "explicit",
                    "ambiguous_or_technical", "preserve", source, source))

ids = [item.split("\t", 1)[0] for item in rows]
assert len(rows) >= 300, len(rows)
assert len(ids) == len(set(ids))
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
with OUTPUT.open("w", encoding="utf-8", newline="\n") as output:
    output.write(HEADER + "\n" + "\n".join(rows) + "\n")
print(f"wrote {len(rows)} cases to {OUTPUT}")
