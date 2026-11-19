// Bloom Forge — многопроходное свечение для After Effects (ScriptUI-панель).
// Строит стек встроенных эффектов Glow с экспоненциально растущим радиусом
// и связывает их выражениями с общими контроллерами "BF ..." на слое.
(function bloomForge(thisObj) {
    var SCRIPT_NAME = "Makiglow";
    var PREFIX = "BF ";
    var CTRL = {
        radius: "BF Radius",
        intensity: "BF Intensity",
        threshold: "BF Threshold",
        falloff: "BF Falloff",
        spread: "BF Spread",
        tint: "BF Tint",
        color: "BF Color"
    };

    var PRESETS = [
        {name: "Мягкое", passes: 6, radius: 150, intensity: 1.5, threshold: 50, falloff: 0.4, spread: 2},
        {name: "Неон",   passes: 7, radius: 300, intensity: 2.5, threshold: 30, falloff: 0.3, spread: 2},
        {name: "Блум",   passes: 8, radius: 600, intensity: 1.2, threshold: 70, falloff: 0.6, spread: 2.2},
        {name: "Тонкое", passes: 4, radius: 60,  intensity: 1.0, threshold: 60, falloff: 0.5, spread: 2}
    ];

    var state = {color: [1, 0.55, 0.15], tint: false};

    // ---------- работа со слоями ----------

    function getComp() {
        var comp = app.project.activeItem;
        return (comp && comp instanceof CompItem) ? comp : null;
    }

    // После addProperty ссылки на другие эффекты могут стать невалидными,
    // поэтому группу эффектов каждый раз берём заново.
    function effects(layer) {
        return layer.property("ADBE Effect Parade");
    }

    function prop(fx, matchName, index) {
        var p = fx.property(matchName);
        return p ? p : fx.property(index);
    }

    function setExpr(p, expr) {
        if (p && p.canSetExpression) p.expression = expr;
    }

    function addControl(layer, matchName, name, value) {
        var fx = effects(layer).addProperty(matchName);
        fx.name = name;
        fx.property(1).setValue(value);
    }

    function removeGlow(layer) {
        var fxGroup = effects(layer);
        if (!fxGroup) return;
        for (var i = fxGroup.numProperties; i >= 1; i--) {
            if (fxGroup.property(i).name.indexOf(PREFIX) === 0) fxGroup.property(i).remove();
        }
    }

    function addGlowPass(layer, i, n, useTint) {
        var fx = effects(layer).addProperty("ADBE Glo2");
        fx.name = PREFIX + "Glow " + (i + 1);

        setExpr(prop(fx, "ADBE Glo2-0002", 2),
            'effect("' + CTRL.threshold + '")(1);');

        // Самый маленький проход первый, самый большой (= BF Radius) последний.
        setExpr(prop(fx, "ADBE Glo2-0003", 3),
            'var s = Math.max(effect("' + CTRL.spread + '")(1), 1.01);\n' +
            'effect("' + CTRL.radius + '")(1) / Math.pow(s, ' + (n - 1 - i) + ');');

        // Веса проходов нормированы, чтобы общая яркость не зависела от их количества.
        setExpr(prop(fx, "ADBE Glo2-0004", 4),
            'var f = clamp(effect("' + CTRL.falloff + '")(1), 0, 0.95), sum = 0;\n' +
            'for (var k = 0; k < ' + n + '; k++) sum += Math.pow(1 - f, k);\n' +
            'effect("' + CTRL.intensity + '")(1) * Math.pow(1 - f, ' + i + ') / sum;');

        prop(fx, "ADBE Glo2-0005", 5).setValue(2); // Composite Original: Behind

        var colors = prop(fx, "ADBE Glo2-0007", 7); // 1 = Original Colors, 2 = A & B Colors
        try {
            colors.expression = 'effect("' + CTRL.tint + '")(1) == 1 ? 2 : 1;';
        } catch (e) {
            colors.setValue(useTint ? 2 : 1);
        }

        // Цвет A — светлое «ядро», цвет B — сам оттенок.
        setExpr(prop(fx, "ADBE Glo2-0012", 12),
            'var c = effect("' + CTRL.color + '")(1);\n[(c[0] + 1) / 2, (c[1] + 1) / 2, (c[2] + 1) / 2, 1];');
        setExpr(prop(fx, "ADBE Glo2-0013", 13),
            'effect("' + CTRL.color + '")(1);');
    }

    function applyGlow(layer, opts) {
        removeGlow(layer);
        addControl(layer, "ADBE Slider Control", CTRL.radius, opts.radius);
        addControl(layer, "ADBE Slider Control", CTRL.intensity, opts.intensity);
        addControl(layer, "ADBE Slider Control", CTRL.threshold, opts.threshold);
        addControl(layer, "ADBE Slider Control", CTRL.falloff, opts.falloff);
        addControl(layer, "ADBE Slider Control", CTRL.spread, opts.spread);
        addControl(layer, "ADBE Checkbox Control", CTRL.tint, opts.tint ? 1 : 0);
        addControl(layer, "ADBE Color Control", CTRL.color, opts.color);
        for (var i = 0; i < opts.passes; i++) addGlowPass(layer, i, opts.passes, opts.tint);
    }

    function selectedEffectLayers(comp) {
        var result = [];
        var sel = comp.selectedLayers;
        for (var i = 0; i < sel.length; i++) {
            if (effects(sel[i])) result.push(sel[i]);
        }
        return result;
    }

    function withUndo(fn) {
        var comp = getComp();
        if (!comp) {
            alert("Сначала откройте композицию.", SCRIPT_NAME);
            return;
        }
        app.beginUndoGroup(SCRIPT_NAME);
        try {
            fn(comp);
        } catch (e) {
            alert("Ошибка: " + e.toString() + (e.line ? " (строка " + e.line + ")" : ""), SCRIPT_NAME);
        } finally {
            app.endUndoGroup();
        }
    }

    // ---------- цвет ----------

    function rgbToHex(c) {
        return (Math.round(c[0] * 255) << 16) | (Math.round(c[1] * 255) << 8) | Math.round(c[2] * 255);
    }

    function hexToRgb(h) {
        return [((h >> 16) & 255) / 255, ((h >> 8) & 255) / 255, (h & 255) / 255];
    }

    // ---------- интерфейс ----------

    function buildUI(thisObj) {
        var win = (thisObj instanceof Panel)
            ? thisObj
            : new Window("palette", SCRIPT_NAME, undefined, {resizeable: true});
        win.orientation = "column";
        win.alignChildren = ["fill", "top"];
        win.spacing = 6;
        win.margins = 10;

        var presetRow = win.add("group");
        presetRow.add("statictext", undefined, "Пресет:").preferredSize.width = 90;
        var presetNames = [];
        for (var p = 0; p < PRESETS.length; p++) presetNames.push(PRESETS[p].name);
        var presetList = presetRow.add("dropdownlist", undefined, presetNames);

        var params = win.add("panel", undefined, "Параметры");
        params.alignChildren = ["fill", "top"];
        params.margins = [10, 14, 10, 10];

        function field(label, value, tip) {
            var g = params.add("group");
            var t = g.add("statictext", undefined, label);
            t.preferredSize.width = 90;
            var e = g.add("edittext", undefined, String(value));
            e.characters = 7;
            if (tip) t.helpTip = e.helpTip = tip;
            return e;
        }

        var passRow = params.add("group");
        passRow.add("statictext", undefined, "Проходов:").preferredSize.width = 90;
        var passItems = [];
        for (var n = 3; n <= 10; n++) passItems.push(String(n));
        var passList = passRow.add("dropdownlist", undefined, passItems);
        passList.helpTip = "Сколько слоёв Glow в стеке. Больше — мягче хвост, но медленнее рендер.";

        var radiusField = field("Радиус:", 150, "Радиус самого большого прохода, px");
        var intensityField = field("Яркость:", 1.5, "Общая интенсивность свечения");
        var thresholdField = field("Порог, %:", 50, "Ниже — светится больше областей");
        var falloffField = field("Затухание:", 0.4, "0..0.95 — насколько тусклее большие проходы");
        var spreadField = field("Шаг радиуса:", 2, "Во сколько раз радиус растёт от прохода к проходу");

        var colorRow = params.add("group");
        var tintCheck = colorRow.add("checkbox", undefined, "Свой цвет");
        tintCheck.preferredSize.width = 90;
        var swatch = colorRow.add("button", undefined, "");
        swatch.preferredSize = [48, 20];
        swatch.helpTip = "Нажмите, чтобы выбрать цвет";
        swatch.onDraw = function () {
            var g = this.graphics;
            var c = state.color;
            g.rectPath(0, 0, this.size[0], this.size[1]);
            g.fillPath(g.newBrush(g.BrushType.SOLID_COLOR, [c[0], c[1], c[2], 1]));
        };
        swatch.onClick = function () {
            var picked = $.colorPicker(rgbToHex(state.color));
            if (picked >= 0) {
                state.color = hexToRgb(picked);
                tintCheck.value = true;
                swatch.hide();
                swatch.show();
            }
        };

        var applyBtn = win.add("button", undefined, "Применить к выделенным слоям");
        var adjBtn = win.add("button", undefined, "Новый корректирующий слой");
        var removeBtn = win.add("button", undefined, "Убрать свечение");

        var hint = win.add("statictext", undefined,
            "Совет: 32 bpc в настройках проекта даёт заметно более мягкое свечение. " +
            "Всё настраивается потом ползунками BF в Effect Controls.", {multiline: true});
        hint.preferredSize.height = 44;

        function loadPreset(preset) {
            passList.selection = preset.passes - 3;
            radiusField.text = String(preset.radius);
            intensityField.text = String(preset.intensity);
            thresholdField.text = String(preset.threshold);
            falloffField.text = String(preset.falloff);
            spreadField.text = String(preset.spread);
        }

        function num(edit, fallback, min, max) {
            var v = parseFloat(String(edit.text).replace(",", "."));
            if (isNaN(v)) v = fallback;
            return Math.min(Math.max(v, min), max);
        }

        function readOptions() {
            return {
                passes: passList.selection ? passList.selection.index + 3 : 6,
                radius: num(radiusField, 150, 1, 5000),
                intensity: num(intensityField, 1.5, 0, 50),
                threshold: num(thresholdField, 50, 0, 100),
                falloff: num(falloffField, 0.4, 0, 0.95),
                spread: num(spreadField, 2, 1.05, 5),
                tint: tintCheck.value,
                color: state.color
            };
        }

        presetList.onChange = function () {
            if (presetList.selection) loadPreset(PRESETS[presetList.selection.index]);
        };

        applyBtn.onClick = function () {
            withUndo(function (comp) {
                var layers = selectedEffectLayers(comp);
                if (!layers.length) {
                    alert("Выделите один или несколько слоёв.", SCRIPT_NAME);
                    return;
                }
                var opts = readOptions();
                for (var i = 0; i < layers.length; i++) applyGlow(layers[i], opts);
            });
        };

        adjBtn.onClick = function () {
            withUndo(function (comp) {
                var layer = comp.layers.addSolid([1, 1, 1], "Makiglow", comp.width, comp.height,
                    comp.pixelAspect, comp.duration);
                layer.adjustmentLayer = true;
                applyGlow(layer, readOptions());
            });
        };

        removeBtn.onClick = function () {
            withUndo(function (comp) {
                var layers = selectedEffectLayers(comp);
                for (var i = 0; i < layers.length; i++) removeGlow(layers[i]);
            });
        };

        presetList.selection = 0;

        win.onResizing = win.onResize = function () { this.layout.resize(); };
        if (win instanceof Window) {
            win.center();
            win.show();
        } else {
            win.layout.layout(true);
        }
        return win;
    }

    buildUI(thisObj);
})(this);
