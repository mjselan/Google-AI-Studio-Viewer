// Qt Installer Framework control script for Google AI Studio Viewer (Windows).
//
// Upgrade handling: IFW refuses to install into a directory that already
// contains an installation ("TargetDirectoryInUse" is a hard error with no
// override). When the default target directory holds a previous install of
// this app, offer to remove it with its own MaintenanceTool and then
// continue with a fresh install into the same folder.
//
// User data is safe: the app cache lives in %APPDATA% (QStandardPaths
// AppDataLocation), not in the install directory.

function Controller()
{
}

Controller.prototype.IntroductionPageCallback = function()
{
    if (installer.isInstaller() !== true)
        return;
    if (installer.value("GaivUpgradeHandled") === "true")
        return;
    installer.setValue("GaivUpgradeHandled", "true");

    var targetDir = installer.value("TargetDir");
    var maintenanceTool = targetDir + "/MaintenanceTool.exe";

    // Does the default target dir contain a previous installation?
    var check = installer.execute("cmd", ["/c", "if exist \"" + maintenanceTool + "\" (exit 0) else (exit 1)"]);
    if (check !== 0)
        return;

    var answer = QMessageBox.question(
        "gaiv.upgrade",
        "Previous installation found",
        "A previous installation of Google AI Studio Viewer was found in:<br><br>" +
        targetDir +
        "<br><br>Remove it now and install the new version into the same folder?",
        QMessageBox.Yes | QMessageBox.No,
        QMessageBox.No);
    if (answer !== QMessageBox.Yes)
        return;

    // Make sure the old binaries are not locked by a running instance.
    // taskkill reports 128 when nothing was running; either outcome is fine.
    installer.execute("taskkill", ["/F", "/IM", "GoogleAIViewer.exe"]);

    var rc = installer.execute(maintenanceTool, ["--confirm-command", "--default-answer", "purge"]);
    if (rc === 0) {
        QMessageBox.information(
            "gaiv.upgrade.done",
            "Previous installation removed",
            "The previous version was removed. Setup will now continue.",
            QMessageBox.Ok);
    } else {
        QMessageBox.warning(
            "gaiv.upgrade.failed",
            "Automatic removal failed",
            "Could not remove the previous installation (code " + rc + ").<br><br>" +
            "Please uninstall it via Settings &gt; Apps, or choose a different folder on the next page.",
            QMessageBox.Ok);
    }
};
