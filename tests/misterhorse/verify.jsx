(function () {
    function findComp(project) {
        for (var i = 1; i <= project.numItems; i++) {
            if (project.item(i) instanceof CompItem && project.item(i).comment === "lindobe-misterhorse-test") {
                return project.item(i);
            }
        }
        throw new Error("Test composition not found");
    }

    function countAnimation(group, counts) {
        for (var i = 1; i <= group.numProperties; i++) {
            var property = group.property(i);
            if (property.matchName === "ADBE Marker") continue;
            if (property.propertyType === PropertyType.PROPERTY) {
                counts.keys += property.numKeys;
                if (property.canSetExpression && property.expressionEnabled) counts.expressions++;
            } else {
                countAnimation(property, counts);
            }
        }
    }

    function inspect(comp) {
        var counts = {keys: 0, expressions: 0, layers: comp.numLayers};
        for (var i = 1; i <= comp.numLayers; i++) countAnimation(comp.layer(i), counts);
        return counts;
    }

    function writeReport(file, text) {
        if (!file.open("w")) throw new Error("Cannot write test report");
        file.write(text);
        file.close();
    }

    var folder = new Folder(Folder.temp.fsName + "/lindobe-misterhorse-test");
    var projectFile = new File(folder.fsName + "/misterhorse-test.aep");
    var report = new File(folder.fsName + "/verification.txt");
    var comp;
    var before;
    var after;
    var item;
    var module;
    var output = new File(folder.fsName + "/misterhorse-test.avi");

    if (!app.project.file || app.project.file.fsName !== projectFile.fsName) {
        throw new Error("Open the Mister Horse test project before verification");
    }
    if (app.project.renderQueue.numItems !== 0) throw new Error("The test render queue must be empty");

    comp = findComp(app.project);
    before = inspect(comp);
    if (before.keys === 0 && before.expressions === 0) throw new Error("Apply an animation before verification");
    app.project.save(projectFile);
    app.open(projectFile);
    comp = findComp(app.project);
    after = inspect(comp);
    if (before.keys !== after.keys || before.expressions !== after.expressions || before.layers !== after.layers) {
        throw new Error("Animation structure changed after saving and reopening");
    }

    item = app.project.renderQueue.items.add(comp);
    try {
        item.timeSpanStart = 0;
        item.timeSpanDuration = comp.duration;
        module = item.outputModule(1);
        module.applyTemplate("Lossless");
        module.file = output;
        writeReport(report, "saveAndReopen=PASS\nkeys=" + after.keys + "\nexpressions=" + after.expressions + "\nrender=STARTED\n");
        app.project.renderQueue.render();
        if (item.status !== RQItemStatus.DONE || !output.exists || output.length === 0) {
            throw new Error("Test render did not complete");
        }
        writeReport(report, "saveAndReopen=PASS\nkeys=" + after.keys + "\nexpressions=" + after.expressions + "\nrender=PASS\n");
    } finally {
        item.remove();
    }
    app.project.save(projectFile);
    comp.openInViewer();
}());
