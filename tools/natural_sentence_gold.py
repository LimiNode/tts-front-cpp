"""Reviewed v0.3.0 natural-sentence gold cases.

This module is deliberately data-only.  Expected normalized/pronunciation text
and diagnostics describe the desired language policy; they are not derived
from tts-front output.
"""


def _case(case_id, language, mode, category, source, normalized, diagnostics="none"):
    return {
        "id": case_id,
        "language": language,
        "mode": mode,
        "category": category,
        "expectation": "preserve" if source == normalized else "normalize",
        "source": source,
        "normalized": normalized,
        "pronunciation": normalized,
        "diagnostics": diagnostics,
    }


def _warning(needle, code="unresolved_number"):
    return (code, needle)


def _full_warning(source, code="ambiguous_normalization"):
    return (code, source)


def natural_sentence_cases():
    cases = []

    en_years = [
        ("1999", "nineteen ninety nine"),
        ("2000", "two thousand"),
        ("2001", "two thousand one"),
        ("2012", "two thousand twelve"),
        ("2020", "two thousand twenty"),
        ("2024", "two thousand twenty four"),
        ("2026", "two thousand twenty six"),
        ("2042", "two thousand forty two"),
    ]
    for index, (surface, spoken) in enumerate(en_years):
        cases.append(_case(f"en-v030-year-{index}-a", "en", "explicit", "year",
                           f"The archive was created in {surface}.",
                           f"The archive was created in {spoken}."))
        cases.append(_case(f"en-v030-year-{index}-b", "en", "explicit", "year",
                           f"Records from {surface} remain available.",
                           f"Records from {spoken} remain available."))

    ru_years = [
        ("1999", "тысяча девятьсот девяносто девятом"),
        ("2000", "двухтысячном"),
        ("2001", "две тысячи первом"),
        ("2012", "две тысячи двенадцатом"),
        ("2020", "две тысячи двадцатом"),
        ("2024", "две тысячи двадцать четвёртом"),
        ("2026", "две тысячи двадцать шестом"),
        ("2042", "две тысячи сорок втором"),
    ]
    for index, (surface, spoken) in enumerate(ru_years):
        cases.append(_case(f"ru-v030-year-{index}-a", "ru", "explicit", "year",
                           f"Архив создан в {surface} году.",
                           f"Архив создан в {spoken} году."))
        cases.append(_case(f"ru-v030-year-{index}-b", "ru", "explicit", "year",
                           f"Отчёт за {surface} год доступен.",
                           f"Отчёт за {spoken} год доступен."))

    en_dates = [
        ("January 1, 2024", "January first, two thousand twenty four", True),
        ("February 29, 2024", "February twenty ninth, two thousand twenty four", True),
        ("March 21, 2025", "March twenty first, two thousand twenty five", True),
        ("December 31, 1999", "December thirty first, nineteen ninety nine", True),
        ("February 29, 2025", None, False),
        ("April 31, 2026", None, False),
        ("June 31, 2024", None, False),
        ("November 31, 2020", None, False),
    ]
    for index, (surface, spoken, valid) in enumerate(en_dates):
        for variant, prefix in enumerate(("The appointment is on ", "Please confirm ")):
            source = f"{prefix}{surface}."
            if valid:
                normalized = f"{prefix}{spoken}."
                diagnostics = "none"
            else:
                normalized = source
                diagnostics = _warning(surface)
            cases.append(_case(f"en-v030-date-{index}-{variant}", "en", "explicit", "date",
                               source, normalized, diagnostics))

    ru_dates = [
        ("01.01.2024", "первое января две тысячи двадцать четвёртого года", True),
        ("29.02.2024", "двадцать девятое февраля две тысячи двадцать четвёртого года", True),
        ("21.03.2025", "двадцать первое марта две тысячи двадцать пятого года", True),
        ("31.12.1999", "тридцать первое декабря тысяча девятьсот девяносто девятого года", True),
        ("29.02.2025", None, False),
        ("31.04.2026", None, False),
        ("31.06.2024", None, False),
        ("31.11.2020", None, False),
    ]
    for index, (surface, spoken, valid) in enumerate(ru_dates):
        for variant, prefix in enumerate(("Встреча назначена на ", "Подтвердите дату ")):
            source = f"{prefix}{surface}."
            if valid:
                normalized = f"{prefix}{spoken}."
                diagnostics = "none"
            else:
                normalized = source
                diagnostics = _warning(surface)
            cases.append(_case(f"ru-v030-date-{index}-{variant}", "ru", "explicit", "date",
                               source, normalized, diagnostics))

    en_units = [
        ("1 kg", "one kilogram"),
        ("2 kg", "two kilograms"),
        ("1.5 kg", "one point five kilograms"),
        ("12 km", "twelve kilometers"),
        ("250 m", "two hundred fifty meters"),
        ("21 cm", "twenty one centimeters"),
        ("2 GB", "two gigabytes"),
        ("1.5 MB", "one point five megabytes"),
    ]
    en_unit_templates = [("The package weighs {}.", "The package weighs {}."),
                         ("The recorded limit is {}.", "The recorded limit is {}.")]
    for index, (surface, spoken) in enumerate(en_units):
        for variant, (source_template, expected_template) in enumerate(en_unit_templates):
            cases.append(_case(f"en-v030-unit-{index}-{variant}", "en", "explicit",
                               "measurement", source_template.format(surface),
                               expected_template.format(spoken)))

    ru_units = [
        ("1 кг", "один килограмм"),
        ("2 кг", "два килограмма"),
        ("1,5 кг", "одна целая пять десятых килограмма"),
        ("12 км", "двенадцать километров"),
        ("250 м", "двести пятьдесят метров"),
        ("21 см", "двадцать один сантиметр"),
        ("2 ГБ", "два гигабайта"),
        ("1,5 МБ", "одна целая пять десятых мегабайта"),
    ]
    ru_unit_templates = [("Посылка весит {}.", "Посылка весит {}."),
                         ("Зафиксированный предел — {}.", "Зафиксированный предел — {}.")]
    for index, (surface, spoken) in enumerate(ru_units):
        for variant, (source_template, expected_template) in enumerate(ru_unit_templates):
            cases.append(_case(f"ru-v030-unit-{index}-{variant}", "ru", "explicit",
                               "measurement", source_template.format(surface),
                               expected_template.format(spoken)))

    en_money = [
        ("$1", "one dollar"),
        ("$2", "two dollars"),
        ("$5", "five dollars"),
        ("$12.50", "twelve dollars fifty cents"),
        ("$21.01", "twenty one dollars one cent"),
        ("$100", "one hundred dollars"),
        ("$0.50", "zero dollars fifty cents"),
        ("($7.25)", "(seven dollars twenty five cents)"),
    ]
    for index, (surface, spoken) in enumerate(en_money):
        cases.append(_case(f"en-v030-money-{index}-a", "en", "explicit", "currency",
                           f"The final price is {surface}.", f"The final price is {spoken}."))
        cases.append(_case(f"en-v030-money-{index}-b", "en", "explicit", "currency",
                           f"Accounting recorded {surface} today.",
                           f"Accounting recorded {spoken} today."))

    ru_money = [
        ("1 руб.", "один рубль"),
        ("2 руб.", "два рубля"),
        ("5 руб.", "пять рублей"),
        ("21 руб.", "двадцать один рубль"),
        ("42 руб.", "сорок два рубля"),
        ("100 руб.", "сто рублей"),
        ("1 250 руб.", "одна тысяча двести пятьдесят рублей"),
        ("2,50 руб.", "две целых пять десятых рубля"),
    ]
    for index, (surface, spoken) in enumerate(ru_money):
        cases.append(_case(f"ru-v030-money-{index}-a", "ru", "explicit", "currency",
                           f"Итоговая цена — {surface}", f"Итоговая цена — {spoken}."))
        cases.append(_case(f"ru-v030-money-{index}-b", "ru", "explicit", "currency",
                           f"Бухгалтерия учла {surface} сегодня.",
                           f"Бухгалтерия учла {spoken} сегодня."))

    en_percent = [
        ("0%", "zero percent"),
        ("1%", "one percent"),
        ("2%", "two percent"),
        ("5%", "five percent"),
        ("21%", "twenty one percent"),
        ("42%", "forty two percent"),
        ("99.5%", "ninety nine point five percent"),
        ("100%", "one hundred percent"),
    ]
    for index, (surface, spoken) in enumerate(en_percent):
        cases.append(_case(f"en-v030-percent-{index}-a", "en", "explicit", "percent",
                           f"Progress reached {surface}.", f"Progress reached {spoken}."))
        cases.append(_case(f"en-v030-percent-{index}-b", "en", "explicit", "percent",
                           f"A {surface} change was reported.",
                           f"A {spoken} change was reported."))

    ru_percent = [
        ("0%", "ноль процентов"),
        ("1%", "один процент"),
        ("2%", "два процента"),
        ("5%", "пять процентов"),
        ("21%", "двадцать один процент"),
        ("42%", "сорок два процента"),
        ("99,5%", "девяносто девять целых пять десятых процента"),
        ("100%", "сто процентов"),
    ]
    for index, (surface, spoken) in enumerate(ru_percent):
        cases.append(_case(f"ru-v030-percent-{index}-a", "ru", "explicit", "percent",
                           f"Прогресс достиг {surface}.", f"Прогресс достиг {spoken}."))
        cases.append(_case(f"ru-v030-percent-{index}-b", "ru", "explicit", "percent",
                           f"Зафиксировано изменение на {surface}.",
                           f"Зафиксировано изменение на {spoken}."))

    en_times = [
        ("0:00", "zero hours zero minutes"),
        ("1:01", "one hour one minute"),
        ("2:02", "two hours two minutes"),
        ("8:05", "eight hours five minutes"),
        ("12:30", "twelve hours thirty minutes"),
        ("18:45", "eighteen hours forty five minutes"),
        ("21:21", "twenty one hours twenty one minutes"),
        ("23:59", "twenty three hours fifty nine minutes"),
    ]
    for index, (surface, spoken) in enumerate(en_times):
        cases.append(_case(f"en-v030-time-{index}-a", "en", "explicit", "time",
                           f"The timer stopped at {surface}.", f"The timer stopped at {spoken}."))
        cases.append(_case(f"en-v030-time-{index}-b", "en", "explicit", "time",
                           f"At {surface}, the alarm sounded.", f"At {spoken}, the alarm sounded."))

    ru_times = [
        ("0:00", "ноль часов ноль минут"),
        ("1:01", "один час одна минута"),
        ("2:02", "два часа две минуты"),
        ("8:05", "восемь часов пять минут"),
        ("12:30", "двенадцать часов тридцать минут"),
        ("18:45", "восемнадцать часов сорок пять минут"),
        ("21:21", "двадцать один час двадцать одна минута"),
        ("23:59", "двадцать три часа пятьдесят девять минут"),
    ]
    for index, (surface, spoken) in enumerate(ru_times):
        cases.append(_case(f"ru-v030-time-{index}-a", "ru", "explicit", "time",
                           f"Таймер остановился на {surface}.",
                           f"Таймер остановился на {spoken}."))
        cases.append(_case(f"ru-v030-time-{index}-b", "ru", "explicit", "time",
                           f"В {surface} прозвучал сигнал.", f"В {spoken} прозвучал сигнал."))

    en_mixed = [
        ("OpenAI API processed 2 запроса.", "OpenAI API processed два запроса."),
        ("Use 3 кг of material.", "Use три килограмма of material."),
        ("RTX-4090 completed 4 задачи.", "RTX-4090 completed четыре задачи."),
        ("HTTP/2 returned 5 ошибок.", "HTTP/2 returned пять ошибок."),
        ("Send 2 отчёта to dev@example.com.", "Send два отчёта to dev@example.com."),
        ("The CUDA job used 8 ГБ памяти.", "The CUDA job used восемь гигабайт памяти."),
        ("Build 7 passed в CI.", "Build seven passed в CI."),
        ("Version V2.1.0 includes 3 исправления.",
         "Version V2.1.0 includes три исправления."),
    ]
    for index, (source, normalized) in enumerate(en_mixed):
        for variant, suffix in enumerate(("", " Проверка завершена.")):
            full_source = source + suffix
            full_normalized = normalized + suffix
            cases.append(_case(f"en-v030-mixed-{index}-{variant}", "en", "auto_segment",
                               "mixed_language", full_source, full_normalized,
                               _full_warning(full_source)))

    ru_mixed = [
        ("OpenAI API обработал 2 requests.", "OpenAI API обработал two requests."),
        ("Используйте 3 kg материала.", "Используйте three kilograms материала."),
        ("RTX-4090 завершил 4 tasks.", "RTX-4090 завершил four tasks."),
        ("HTTP/2 вернул 5 errors.", "HTTP/2 вернул five errors."),
        ("Отправьте 2 reports на dev@example.com.",
         "Отправьте two reports на dev@example.com."),
        ("Задача CUDA использовала 8 GB памяти.",
         "Задача CUDA использовала eight gigabytes памяти."),
        ("Сборка 7 прошла в CI.", "Сборка семь прошла в CI."),
        ("Версия V2.1.0 содержит 3 fixes.", "Версия V2.1.0 содержит three fixes."),
    ]
    for index, (source, normalized) in enumerate(ru_mixed):
        for variant, suffix in enumerate(("", " Validation complete.")):
            full_source = source + suffix
            full_normalized = normalized + suffix
            cases.append(_case(f"ru-v030-mixed-{index}-{variant}", "ru", "auto_segment",
                               "mixed_language", full_source, full_normalized,
                               _full_warning(full_source)))

    en_controls = [
        ("Values 1, 2, and 3 were accepted.", "Values one, two, and three were accepted.", "punctuation", "none"),
        ("Choose option (2), then press 4.", "Choose option (two), then press four.", "punctuation", "none"),
        ("The range [3]–[5] is reserved.", "The range [three]–[five] is reserved.", "punctuation", "none"),
        ("She entered “42”; he entered 7.", "She entered “forty two”; he entered seven.", "punctuation", "none"),
        ("Keep https://example.com/v2.1.0 unchanged.", None, "ambiguous_or_technical", "none"),
        ("Email user42@example.com after 2 attempts.", "Email user42@example.com after two attempts.", "ambiguous_or_technical", "none"),
        ("The identifier RTX-4090 remains unchanged.", None, "ambiguous_or_technical", "none"),
        ("The token C++17 remains unchanged.", None, "ambiguous_or_technical", "none"),
        ("Malformed 1.2..3 must be preserved.", None, "ambiguous_or_technical", _warning("1.2..3")),
        ("Malformed 1:02::3 must be preserved.", None, "ambiguous_or_technical", _warning("1:02::3")),
        ("Keep +1e+3#4 as written.", None, "ambiguous_or_technical", _warning("+1e+3#4")),
        ("Keep −1e—3$4 as written.", None, "ambiguous_or_technical", _warning("−1e—3$4")),
        ("The value 1.5%RTX- 4090 is unsupported.", None, "ambiguous_or_technical", _warning("1.5%RTX- 4090")),
        ("The value 1.5%RTX− 4090 is unsupported.", None, "ambiguous_or_technical", _warning("1.5%RTX− 4090")),
        ("Keep 24:00 unchanged.", None, "time", _warning("24:00")),
        ("Keep February 30, 2024 unchanged.", None, "date", _warning("February 30, 2024")),
    ]
    for index, (source, normalized, category, diagnostics) in enumerate(en_controls):
        cases.append(_case(f"en-v030-control-{index}", "en", "explicit", category, source,
                           source if normalized is None else normalized, diagnostics))

    ru_controls = [
        ("Значения 1, 2 и 3 приняты.", "Значения один, два и три приняты.", "punctuation", "none"),
        ("Выберите вариант (2), затем нажмите 4.", "Выберите вариант (два), затем нажмите четыре.", "punctuation", "none"),
        ("Диапазон [3]–[5] зарезервирован.", "Диапазон [три]–[пять] зарезервирован.", "punctuation", "none"),
        ("Она ввела «42»; он ввёл 7.", "Она ввела «сорок два»; он ввёл семь.", "punctuation", "none"),
        ("После 2 попыток система ответила.", "После двух попыток система ответила.", "grammar", "none"),
        ("После 22 попыток система ответила.", "После двадцати двух попыток система ответила.", "grammar", "none"),
        ("Найдено 2 ошибки.", "Найдено две ошибки.", "grammar", "none"),
        ("Проверено 3 из 5 узлов.", "Проверено три из пяти узлов.", "grammar", "none"),
        ("Ссылка https://example.com/v2.1.0 не меняется.", None, "ambiguous_or_technical", "none"),
        ("Письмо user42@example.com отправлено после 2 попыток.",
         "Письмо user42@example.com отправлено после двух попыток.", "ambiguous_or_technical", "none"),
        ("Идентификатор RTX-4090 остаётся без изменений.", None, "ambiguous_or_technical", "none"),
        ("Токен C++17 остаётся без изменений.", None, "ambiguous_or_technical", "none"),
        ("Некорректное 1,2..3 нужно сохранить.", None, "ambiguous_or_technical", _warning("1,2..3")),
        ("Некорректное 1:02::3 нужно сохранить.", None, "ambiguous_or_technical", _warning("1:02::3")),
        ("Значение 1.5%RTX- 4090 не поддерживается.", None, "ambiguous_or_technical", _warning("1.5%RTX- 4090")),
        ("Значение 1.5%RTX− 4090 не поддерживается.", None, "ambiguous_or_technical", _warning("1.5%RTX− 4090")),
    ]
    for index, (source, normalized, category, diagnostics) in enumerate(ru_controls):
        cases.append(_case(f"ru-v030-control-{index}", "ru", "explicit", category, source,
                           source if normalized is None else normalized, diagnostics))

    assert len(cases) == 256, len(cases)
    assert sum(case["language"] == "en" for case in cases) == 128
    assert sum(case["language"] == "ru" for case in cases) == 128
    return cases
