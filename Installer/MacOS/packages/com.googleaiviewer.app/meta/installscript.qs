// Qt Installer Framework component script for Google AI Studio Viewer (macOS).
// The default DMG is drag-and-drop; this script is used for --ifw installs.

function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (systemInfo.productType === "macos") {
        // Ensure the .app is executable in the installed payload.
        component.addOperation(
            "Execute", "chmod", "-R", "+x", "@TargetDir@/GoogleAIViewer.app"
        );
    }
};
