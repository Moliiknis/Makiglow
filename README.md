# Makiglow

[English](#english) · [Русский](#русский)

## English

An Adobe After Effects plug-in from the **Makimchi** set. Glow effect: a soft multi-layer glow with threshold, radius, exposure, falloff and tint, computed in linear color, 8/16/32 bpc.

Similar to: **Deep Glow**.

> Written from scratch. Not affiliated with the authors of the original plug-in and contains none of their code.

### Install

1. Download `Makiglow-1.0.1-Setup.exe` from [Releases](https://github.com/Moliiknis/Makiglow/releases/latest).
2. Close After Effects and run the installer. It asks for administrator rights because the effect goes into the After Effects folder.
3. Start After Effects and open Effect > Makimchi > Makiglow.

Windows may show a SmartScreen warning because the installer is not code-signed: click "More info" → "Run anyway".

Works in After Effects 2022 and later, Windows 10/11 x64.

To uninstall: **Settings → Apps → Makiglow for After Effects**.

### Build from source

Requires Visual Studio 2022+ with "Desktop development with C++" and the [After Effects SDK](https://developer.adobe.com/after-effects/). By default the SDK is looked up in `%USERPROFILE%\Downloads\AfterEffectsSDK_27.0.0_win\AfterEffectsSDK\Examples`; set the `AESDK` environment variable to use another path.

```powershell
.\build-installer.ps1   # builds the plug-in and release\Makiglow-1.0.1-Setup.exe
```

### License

[PolyForm Noncommercial 1.0.0](LICENSE.md): free for personal and any noncommercial use, you may modify and share it. Commercial use (selling, paid client work, use in a company for profit) is not permitted by this license.

---

## Русский

Плагин для Adobe After Effects из набора **Makimchi**. Эффект свечения: мягкий многослойный glow с порогом, радиусом, экспозицией, затуханием и оттенком, расчёт в линейном цвете, 8/16/32 bpc.

Аналог: **Deep Glow**.

> Собственная реализация с нуля. Не связан с авторами оригинального плагина и не содержит их кода.

### Установка

1. Нужно скачать `Makiglow-1.0.1-Setup.exe` со страницы [Releases](https://github.com/Moliiknis/Makiglow/releases/latest).
2. Нужно закрыть After Effects и запустить установщик. Он запросит права администратора, потому что эффект ставится в папку After Effects.
3. Нужно открыть After Effects: Эффект → Makimchi → Makiglow.

Windows может показать предупреждение SmartScreen, потому что установщик не подписан сертификатом. В этом случае нужно нажать «Подробнее» → «Выполнить в любом случае».

Работает в After Effects 2022 и новее, Windows 10/11 x64.

Для удаления нужно открыть **Параметры → Приложения → Makiglow for After Effects**.

### Сборка из исходников

Нужны Visual Studio 2022+ с «Desktop development with C++» и [After Effects SDK](https://developer.adobe.com/after-effects/). По умолчанию SDK ищется в `%USERPROFILE%\Downloads\AfterEffectsSDK_27.0.0_win\AfterEffectsSDK\Examples`, другой путь нужно задать в переменной окружения `AESDK`.

```powershell
.\build-installer.ps1   # собирает плагин и release\Makiglow-1.0.1-Setup.exe
```

### Лицензия

[PolyForm Noncommercial 1.0.0](LICENSE.md): бесплатно для личного и любого некоммерческого использования, можно изменять и делиться. Коммерческое использование (продажа, платные заказы, использование в компании ради прибыли) этой лицензией не разрешено.
