# Makiglow

Плагин для Adobe After Effects из набора **Makimchi**. Эффект свечения: мягкий многослойный glow с порогом, радиусом, экспозицией, затуханием и оттенком, расчёт в линейном цвете, 8/16/32 bpc.

Аналог: **Deep Glow**.

> Собственная реализация с нуля. Не связан с авторами оригинального плагина и не содержит их кода.

## Установка

1. Скачай `Makiglow-1.0.0-Setup.exe` со страницы [Releases](https://github.com/Moliiknis/Makiglow/releases/latest).
2. Закрой After Effects и запусти установщик. Он попросит права администратора, потому что эффект ставится в папку After Effects.
3. Открой After Effects: Эффект → Makimchi → Makiglow.

Windows может показать предупреждение SmartScreen, потому что установщик не подписан сертификатом: «Подробнее» → «Выполнить в любом случае».

Работает в After Effects 2022 и новее, Windows 10/11 x64.

Удалить: **Параметры → Приложения → Makiglow for After Effects**.

## Сборка из исходников

Нужны Visual Studio 2022+ с «Desktop development with C++» и [After Effects SDK](https://developer.adobe.com/after-effects/). По умолчанию SDK ищется в `%USERPROFILE%\Downloads\AfterEffectsSDK_27.0.0_win\AfterEffectsSDK\Examples`, другой путь задаётся переменной окружения `AESDK`.

```powershell
.\build-installer.ps1   # собрать плагин и release\Makiglow-1.0.0-Setup.exe
```

## Лицензия

[PolyForm Noncommercial 1.0.0](LICENSE.md): бесплатно для личного и любого некоммерческого использования, можно изменять и делиться. Коммерческое использование (продажа, платные заказы, использование в компании ради прибыли) этой лицензией не разрешено.
