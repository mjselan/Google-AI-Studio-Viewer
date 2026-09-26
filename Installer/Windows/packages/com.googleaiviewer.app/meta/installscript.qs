// Qt Installer Framework component script for Google AI Studio Viewer.
// Creates Start Menu + Desktop shortcuts on Windows.

function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (systemInfo.productType === "windows") {
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/GoogleAIViewer.exe",
            "@StartMenuDir@/Google AI Studio Viewer.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/GoogleAIViewer.exe",
            "description=Browse and search Google AI Studio conversations"
        );
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/GoogleAIViewer.exe",
            "@DesktopDir@/Google AI Studio Viewer.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/GoogleAIViewer.exe",
            "description=Browse and search Google AI Studio conversations"
        );
    }
};
