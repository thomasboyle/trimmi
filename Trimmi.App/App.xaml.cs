using Microsoft.UI.Xaml;
using Trimmi_App.Services;
using Trimmi_App.Services.UiSounds;

namespace Trimmi_App;

public partial class App : Application
{
    public static MainWindow? MainWindow { get; private set; }

    public static SettingsStore SettingsStore { get; } = new();

    public App()
    {
        InitializeComponent();
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        var settings = SettingsStore.Load();
        UiSoundService.IsEnabled = settings.UiSoundsEnabled;
        UiSoundService.VolumePercent = settings.UiSoundVolume;

        MainWindow = new MainWindow();
        if (MainWindow.Content is FrameworkElement root)
        {
            root.RequestedTheme = ElementTheme.Light;
        }

        MainWindow.Activate();
    }
}
