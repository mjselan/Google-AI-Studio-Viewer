function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (systemInfo.productType === "windows")
        return;

    // Make the launcher executable in the installed payload.
    component.addOperation(
        "Execute", "chmod", "+x", "@TargetDir@/googleaiviewer.sh",
        "UNDOEXECUTE", "chmod", "-x", "@TargetDir@/googleaiviewer.sh"
    );

    // Install a desktop entry pointing at the real install location.
    var appsDir = installer.environmentVariable("HOME") + "/.local/share/applications";
    var desktopFile = appsDir + "/googleaiviewer.desktop";
    component.addOperation(
        "Execute", "mkdir", "-p", appsDir,
        "UNDOEXECUTE", "rm", "-f", desktopFile
    );
    component.addOperation(
        "CreateDesktopEntry",
        desktopFile,
        "Type=Application\n" +
        "Name=Google AI Studio Viewer\n" +
        "Comment=Browse and search Google AI Studio conversation history\n" +
        "Exec=@TargetDir@/googleaiviewer.sh %F\n" +
        "Icon=@TargetDir@/googleaiviewer.png\n" +
        "Terminal=false\n" +
        "Categories=Utility;Office;\n" +
        "StartupWMClass=GoogleAIViewer\n",
        "UNDOEXECUTE", "rm", "-f", desktopFile
    );
};
