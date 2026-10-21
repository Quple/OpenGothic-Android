// The tab Tests: the tests of the game that run in the game (lib/selftests.js), with buttons and the result of each.
"use strict";

const { createSelfTests } = require("./selftests.js");
const { createFixCases } = require("./fixcases.js");

const Green = [0.45, 0.85, 0.45, 1];
const Red = [0.95, 0.40, 0.35, 1];
const Grey = [0.6, 0.6, 0.6, 1];
const Gold = [0.82, 0.66, 0.34, 1];

class TestsView {
    constructor(env) {
        this.env = env;
        this.suite = null;
        this.results = new Map();    // name -> result
        this.summary = "";
        this.cases = null;
        this.caseResults = new Map();   // name -> { status, message, step }
    }

    ensureCases() {
        if (!this.cases) this.cases = createFixCases(this.env);
        return this.cases;
    }

    runCase(name, check) {
        const cases = this.ensureCases();
        const r = check ? cases.check(name) : cases.run(name);
        this.caseResults.set(name, { ...r, step: check ? "check" : "run" });
        const line = `${r.status.toUpperCase()} ${name}: ${r.message}`;
        if (r.status === "fail") console.error(line); else console.log(line);
    }

    ensure() {
        if (!this.suite) this.suite = createSelfTests(this.env);
        return this.suite;
    }

    run(word) {
        const suite = this.ensure();
        const r = suite.run(word);
        for (const x of r.results) this.results.set(x.name, x);
        this.summary = `${r.pass} passed, ${r.fail} failed, ${r.skip} skipped`;
        for (const x of r.results) if (x.status === "fail") console.error(`TEST FAILED [${x.area}] ${x.name}: ${x.message}`);
        console.log(`Tests (${word || "all"}): ${this.summary}`);
    }

    runOne(t) {
        const x = this.ensure().runOne(t);
        this.results.set(x.name, x);
        if (x.status === "fail") console.error(`TEST FAILED [${x.area}] ${x.name}: ${x.message}`);
    }

    draw(ui) {
        const suite = this.ensure();
        ui.withColor(Gold, () => ui.text("Tests that run in the game"));
        ui.textDisabled("They change the state of the game and put it back: use a game that can be thrown away. A test that needs something this game does not have is skipped.");
        if (ui.button("Run all##tall", ui.fontSize() * 8, ui.fontSize() * 2)) this.run("all");
        for (const area of suite.areas()) {
            ui.sameLine();
            if (ui.button(area + "##tarea" + area, ui.fontSize() * 6, ui.fontSize() * 2)) this.run(area);
        }
        if (this.summary) ui.withColor(this.summary.includes(" 0 failed") ? Green : Red, () => ui.text(this.summary));
        ui.separator();
        ui.withColor(Gold, () => ui.text("Fixed cases of Chronicles of Myrtana (a case sets itself up in the game that is loaded)"));
        const cases = this.ensureCases();
        ui.child("##cases", -1, ui.fontSize() * 14, () => {
            cases.cases.forEach((c, i) => {
                const r = this.caseResults.get(c.name);
                const color = !r ? Grey : r.status === "pass" ? Green : r.status === "fail" ? Red : r.status === "look" ? Gold : Grey;
                if (ui.button((c.auto ? "Run" : c.manual ? "Show" : "Set up") + "##case" + i, ui.fontSize() * 5, 0)) this.runCase(c.name, false);
                if (c.check) {
                    ui.sameLine();
                    if (ui.button("Check##casec" + i, ui.fontSize() * 5, 0)) this.runCase(c.name, true);
                }
                ui.sameLine();
                ui.withColor(color, () => ui.textWrapped(`${c.name}: ${c.about}`));
                if (r) ui.withColor(color, () => { ui.indent(16); ui.textWrapped(`${r.status.toUpperCase()}${r.step === "check" ? " (check)" : ""}: ${r.message}`); ui.unindent(16); });
            });
        });
        ui.separator();
        ui.child("##tests", -1, 0, () => {
            suite.tests.forEach((t, i) => {
                const r = this.results.get(t.name);
                const color = !r ? Grey : r.status === "pass" ? Green : r.status === "fail" ? Red : Grey;
                const mark = !r ? "  " : r.status === "pass" ? "PASS " : r.status === "fail" ? "FAIL " : "SKIP ";
                ui.withColor(color, () => {
                    if (ui.selectable(`${mark}[${t.area}] ${t.name}##t${i}`, false)) this.runOne(t);
                });
                if (r && r.message) ui.withColor(color, () => { ui.indent(16); ui.textWrapped(r.message); ui.unindent(16); });
            });
        });
    }
}

module.exports = { TestsView };
