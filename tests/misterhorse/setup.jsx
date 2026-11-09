(function () {
    var folder = new Folder(Folder.temp.fsName + "/lindobe-misterhorse-test");
    var project = app.project;
    var comp;
    var layer;
    var report;

    if (!folder.exists && !folder.create()) throw new Error("Cannot create test folder");
    if (project.numItems !== 0) throw new Error("Use an empty project for this test");

    app.beginUndoGroup("Mister Horse smoke test");
    try {
        comp = project.items.addComp("Mister Horse smoke test", 1280, 720, 1, 3, 24);
        comp.comment = "lindobe-misterhorse-test";
        layer = comp.layers.addText("LinDobe");
        layer.name = "Animation target";
        layer.property("ADBE Transform Group").property("ADBE Position").setValue([640, 360]);
        layer.selected = true;
        comp.openInViewer();
        project.save(new File(folder.fsName + "/misterhorse-test.aep"));
        report = new File(folder.fsName + "/setup.txt");
        if (!report.open("w")) throw new Error("Cannot write test report");
        report.write("compId=" + comp.id + "\nlayerId=" + layer.id + "\n");
        report.close();
    } finally {
        app.endUndoGroup();
    }
}());
