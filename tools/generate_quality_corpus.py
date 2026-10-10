#!/usr/bin/env python3
"""Generate the reviewed EN/RU quality corpus without invoking tts-front."""

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "tests" / "quality" / "en_ru_sentences.tsv"
HEADER = "id\tlanguage\tmode\tcategory\texpectation\tinput\texpected\tdiagnostics"


def row(case_id, language, mode, category, expectation, source, expected, diagnostics="none"):
    fields = (case_id, language, mode, category, expectation, source, expected, diagnostics)
    assert all("\t" not in field and "\n" not in field for field in fields)
    assert language in {"en", "ru"}
    assert mode in {"explicit", "auto_segment"}
    assert expectation in {"normalize", "preserve"}
    assert diagnostics == "none" or all(
        "@" in item and ":" in item for item in diagnostics.split(";")
    )
    assert (source == expected) == (expectation == "preserve")
    return "\t".join(fields)


def warning(source, needle, code="unresolved_number"):
    offset = source.encode("utf-8").find(needle.encode("utf-8"))
    assert offset >= 0, (source, needle)
    return f"{code}@{offset}:{len(needle.encode('utf-8'))}"


def warnings(source, *needles):
    return ";".join(warning(source, needle) for needle in needles)


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
    ("en", "-0", "minus zero"),
    ("en", "-0.0", "minus zero point zero"),
    ("ru", "-0", "минус ноль"),
    ("ru", "-0,00", "минус ноль целых ноль сотых"),
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
                    source, warning(source, surface)))

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
    ("ru", "Устройство CUDA 13.3 потребляет 2 GB памяти.",
     "Устройство CUDA 13.3 потребляет two GB памяти."),
    ("ru", "На сервере HTTP/2 работает 4 процесса.",
     "На сервере HTTP/2 работает четыре процесса."),
    ("ru", "OpenAI API обработал 12 запросов.",
     "OpenAI API обработал двенадцать запросов."),
    ("ru", "Порог 0.5% задан для RTX-4090.",
     "Порог ноль point пять процентов задан для RTX-4090."),
    ("ru", "Версия V2.1.0—123 отмечена номером 7.",
     "Версия V2.1.0—123 отмечена номером семь."),
    ("en", "Use 3 кг порошка with OpenAI API.",
     "Use три килограмма порошка with OpenAI API."),
    ("en", "The GPU RTX 4090 needs 8 GB of memory.",
     "The GPU RTX 4090 needs eight gigabytes of memory."),
    ("en", "Build 12 on HTTP/2 passed validation.",
     "Build twelve on HTTP/2 passed validation."),
    ("en", "Deploy 5 replicas в the cluster.",
     "Deploy five replicas в the cluster."),
    ("en", "Version V2.1.0—123 is tagged 7.",
     "Version V2.1.0—123 is tagged seven."),
    ("ru", "У меня -0 kg", "У меня minus zero kilograms"),
    ("ru", "У меня -0.0 kg", "У меня minus zero point zero kg"),
]
for index, (language, source, expected) in enumerate(mixed_cases):
    diagnostics = "none"
    if index in {0, 1, 10, 12, 20, 21} and language == "ru":
        diagnostics = f"ambiguous_normalization@0:{len(source.encode('utf-8'))}"
    if index in {5, 15, 18} and language == "en":
        diagnostics = f"ambiguous_normalization@0:{len(source.encode('utf-8'))}"
    rows.append(row(f"{language}-mixed-{index}", language, "auto_segment", "mixed_language",
                    "normalize", source, expected, diagnostics))

preserved = [
    ("en", "Deploy C++17 to RTX-4090 at https://example.com/v2.1.0.", "none"),
    ("en", "The endpoint HTTP/2 returned code #123.", "none"),
    ("en", "Keep malformed value 1.2..3 unchanged.", None),
    ("en", "Keep malformed time 1:02::3 unchanged.", None),
    ("en", "Identifier V2.1.0—123 must remain atomic.", None),
    ("en", "The malformed amount 1 dollarfoo stays unchanged.", None),
    ("en", "The malformed unit 2 kgfoo stays unchanged.", None),
    ("en", "Invalid clock values 24:00 and 99:99 remain digits.", None),
    ("ru", "Запустите C++17 на RTX-4090 через https://example.com/v2.1.0.", "none"),
    ("ru", "Ответ HTTP/2 содержит код #123.", "none"),
    ("ru", "Некорректное значение 1,2..3 сохраняется.", None),
    ("ru", "Некорректное время 1:02::3 сохраняется.", None),
    ("ru", "Идентификатор V2.1.0—123 остаётся атомарным.", None),
    ("ru", "Некорректная сумма 1 рублейfoo сохраняется.", None),
    ("ru", "Некорректная единица 2 кгfoo сохраняется.", None),
    ("ru", "Ошибочное время 24:00 и 99:99 остаётся цифрами.", None),
]
for index, (language, source, diagnostics) in enumerate(preserved):
    if diagnostics is None:
        if language == "en" and index == 2:
            diagnostics = warning(source, "1.2..3")
        elif language == "en" and index == 3:
            diagnostics = warning(source, "1:02::3")
        elif language == "en" and index == 4:
            diagnostics = warning(source, "V2.1.0—123")
        elif language == "en" and index == 5:
            diagnostics = warning(source, "1 dollarfoo")
        elif language == "en" and index == 6:
            diagnostics = warning(source, "2 kgfoo")
        elif language == "en" and index == 7:
            diagnostics = warnings(source, "24:00", "99:99")
        elif language == "ru" and index == 10:
            diagnostics = warning(source, "1,2..3")
        elif language == "ru" and index == 11:
            diagnostics = warning(source, "1:02::3")
        elif language == "ru" and index == 12:
            diagnostics = warning(source, "V2.1.0—123")
        elif language == "ru" and index == 13:
            diagnostics = warning(source, "1 рублейfoo")
        elif language == "ru" and index == 14:
            diagnostics = warning(source, "2 кгfoo")
        elif language == "ru" and index == 15:
            diagnostics = warnings(source, "24:00", "99:99")
    rows.append(row(f"{language}-preserve-{index}", language, "explicit",
                    "ambiguous_or_technical", "preserve", source, source, diagnostics))

natural_cases = [
    ("en-natural-00", "en", "integer", "normalize", "At 08:05, the train leaves platform 3.",
     "At eight hours five minutes, the train leaves platform three.", "none"),
    ("en-natural-01", "en", "currency", "normalize", "The invoice total is $2.50 before tax.",
     "The invoice total is two dollars fifty cents before tax.", "none"),
    ("en-natural-02", "en", "decimal", "normalize", "The sensor reads -4.5.",
     "The sensor reads minus four point five.", "none"),
    ("en-natural-03", "en", "punctuation", "normalize",
     "Press “2” to continue, or choose option 4.",
     "Press “two” to continue, or choose option four.", "none"),
    ("en-natural-04", "en", "measurement", "normalize", "The package is 1.5 kg.",
     "The package is one point five kilograms.", "none"),
    ("en-natural-05", "en", "decimal", "normalize", "Version 2.1 is newer than version 1.9.",
     "Version two point one is newer than version one point nine.", "none"),
    ("en-natural-06", "en", "percent", "normalize", "The backup completed at 100%.",
     "The backup completed at one hundred percent.", "none"),
    ("en-natural-07", "en", "currency", "normalize",
     "The invoice lists $1.01, $2.50, and $5.",
     "The invoice lists one dollar one cent, two dollars fifty cents, and five dollars.", "none"),
    ("en-natural-08", "en", "measurement", "normalize", "Route A is 12 km; route B is 8 km.",
     "Route A is twelve kilometers; route B is eight kilometers.", "none"),
    ("en-natural-09", "en", "date", "preserve", "The quoted date is 2026-02-01.",
     "The quoted date is 2026-02-01.", warning("The quoted date is 2026-02-01.", "2026-02-01")),
    ("en-natural-10", "en", "currency", "preserve", "Use €7.20 for the foreign-price example.",
     "Use €7.20 for the foreign-price example.", warning("Use €7.20 for the foreign-price example.", "€7.20")),
    ("en-natural-11", "en", "integer", "normalize", "A value in [3] was rejected.",
     "A value in [three] was rejected.", "none"),
    ("ru-natural-00", "ru", "time", "normalize", "К 8:30 подготовьте 3 отчёта.",
     "К восемь часов тридцать минут подготовьте три отчёта.", "none"),
    ("ru-natural-01", "ru", "currency", "normalize", "Стоимость заказа — 1 250 руб.",
     "Стоимость заказа — одна тысяча двести пятьдесят рублей.", "none"),
    ("ru-natural-02", "ru", "decimal", "normalize", "Температура опустилась до -4,5.",
     "Температура опустилась до минус четырёх целых пяти десятых.", "none"),
    ("ru-natural-03", "ru", "punctuation", "normalize", "Нажмите «2», затем выберите 4.",
     "Нажмите «два», затем выберите четыре.", "none"),
    ("ru-natural-04", "ru", "measurement", "normalize", "Расстояние составляет 12,5 км.",
     "Расстояние составляет двенадцать целых пять десятых километра.", "none"),
    ("ru-natural-05", "ru", "measurement", "normalize", "В резерве осталось 1,5 ГБ.",
     "В резерве осталось одна целая пять десятых ГБ.", "none"),
    ("ru-natural-06", "ru", "percent", "normalize", "Доля ошибок — 0,5%.",
     "Доля ошибок — ноль целых пять десятых процента.", "none"),
    ("ru-natural-07", "ru", "date", "normalize", "Срок — 31.12.2025.",
     "Срок — тридцать первое декабря две тысячи двадцать пятого года.", "none"),
    ("ru-natural-08", "ru", "currency", "preserve", "Оплата в евро: €7.",
     "Оплата в евро: €7.", warning("Оплата в евро: €7.", "€7")),
    ("ru-natural-09", "ru", "integer", "normalize", "Проверено 3 из 5 узлов.",
     "Проверено три из пяти узлов.", "none"),
    ("ru-natural-10", "ru", "integer", "normalize", "В 2026 году запланировано 12 релизов.",
     "В две тысячи двадцать шесть году запланировано двенадцать релизов.", "none"),
    ("ru-natural-11", "ru", "integer", "normalize", "Значение равно [42].",
     "Значение равно [сорок два].", "none"),
    ("en-negative-time-00", "en", "time", "preserve", "-0:00", "-0:00", warning("-0:00", "-0:00")),
    ("en-negative-time-01", "en", "time", "preserve", "-1:02", "-1:02", warning("-1:02", "-1:02")),
    ("ru-negative-time-00", "ru", "time", "preserve", "-0:00", "-0:00", warning("-0:00", "-0:00")),
    ("ru-negative-time-01", "ru", "time", "preserve", "-1:02", "-1:02", warning("-1:02", "-1:02")),
    ("en-negative-ordinal-00", "en", "date", "preserve", "-0th", "-0th", warning("-0th", "-0th")),
    ("en-positive-sign-00", "en", "measurement", "preserve", "+1.2 kg", "+1.2 kg",
     warning("+1.2 kg", "+1.2 kg")),
    ("en-positive-sign-01", "en", "percent", "preserve", "+1.2%", "+1.2%",
     warning("+1.2%", "+1.2%")),
    ("en-unicode-minus-time-00", "en", "time", "preserve", "−1:02", "−1:02",
     warning("−1:02", "−1:02")),
    ("ru-positive-sign-00", "ru", "measurement", "preserve", "+1.2 kg", "+1.2 kg",
     warning("+1.2 kg", "+1.2 kg")),
    ("ru-positive-sign-01", "ru", "percent", "preserve", "+1,2%", "+1,2%",
     warning("+1,2%", "+1,2%")),
    ("ru-unicode-minus-time-00", "ru", "time", "preserve", "−1:02", "−1:02",
     warning("−1:02", "−1:02")),
]
for case_id, language, category, expectation, source, expected, diagnostics in natural_cases:
    rows.append(row(case_id, language, "explicit", category, expectation, source, expected,
                    diagnostics))

ids = [item.split("\t", 1)[0] for item in rows]
assert len(rows) >= 340, len(rows)
assert len(ids) == len(set(ids))
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
content = HEADER + "\n" + "\n".join(rows) + "\n"
if "--check" in sys.argv:
    if not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != content:
        print(f"quality corpus is out of date: regenerate {OUTPUT}", file=sys.stderr)
        raise SystemExit(1)
    print(f"quality corpus is up to date: {len(rows)} cases")
    raise SystemExit(0)
with OUTPUT.open("w", encoding="utf-8", newline="\n") as output:
    output.write(content)
print(f"wrote {len(rows)} cases to {OUTPUT}")
