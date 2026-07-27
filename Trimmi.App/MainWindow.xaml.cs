using System.Collections.ObjectModel;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Trimmi.Core.Models;
using Trimmi.Core.Services;
using Trimmi_App.Views;
using Windows.ApplicationModel.DataTransfer;
using Windows.Media.Core;
using Windows.Media.Playback;
using Windows.Storage;
using Windows.Storage.Pickers;
using WinRT.Interop;

namespace Trimmi_App;

public sealed partial class MainWindow : Window
{
    private static readonly HashSet<string> VideoExtensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".mp4", ".mkv", ".mov", ".webm", ".avi", ".m4v", ".wmv", ".flv", ".ts", ".m2ts",
    };

    private static readonly SolidColorBrush DropZoneDefaultFill =
        new(Windows.UI.Color.FromArgb(0xFF, 0xE8, 0xEC, 0xD9));
    private static readonly SolidColorBrush DropZoneHoverFill =
        new(Windows.UI.Color.FromArgb(0xFF, 0xDC, 0xE4, 0xC8));
    private static readonly SolidColorBrush DropZoneDefaultStroke =
        new(Windows.UI.Color.FromArgb(0xFF, 0x4F, 0x58, 0x38));
    private static readonly SolidColorBrush DropZoneHoverStroke =
        new(Windows.UI.Color.FromArgb(0xFF, 0x2A, 0x32, 0x20));

    private readonly EncoderCapabilities _caps = new();
    private readonly MediaProbeService _probe = new();
    private readonly ExportService _exporter = new();
    private readonly ThumbnailService _thumbnails = new();
    private readonly MediaPlayer _mediaPlayer = new();
    private readonly ObservableCollection<BitmapImage> _timelineThumbs = [];

    private VideoMetadata? _metadata;
    private CancellationTokenSource? _thumbCts;
    private CancellationTokenSource? _exportCts;
    private bool _updatingFields;
    private bool _dropZoneHovered;
    private bool _fullscreen;
    private ContentDialog? _progressDialog;
    private ProgressBar? _progressBar;
    private TextBlock? _progressStatus;

    public MainWindow()
    {
        InitializeComponent();
        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);
        AppWindow.Resize(new Windows.Graphics.SizeInt32(1280, 820));
        AppWindow.SetIcon("Assets/AppIcon.ico");

        SystemBackdrop = null;
        PreviewPlayer.SetMediaPlayer(_mediaPlayer);
        _mediaPlayer.Volume = 0.8;
        Timeline.Thumbnails = _timelineThumbs;

        _mediaPlayer.PlaybackSession.PositionChanged += PlaybackSession_PositionChanged;
        _mediaPlayer.PlaybackSession.PlaybackStateChanged += PlaybackSession_PlaybackStateChanged;

        if (Content is UIElement rootContent)
        {
            rootContent.KeyDown += Root_KeyDown;
        }

        SetControlsEnabled(false);

        if (Content is FrameworkElement root)
        {
            root.Loaded += MainWindow_Loaded;
        }
    }

    private void MainWindow_Loaded(object sender, RoutedEventArgs e)
    {
        if (Content is FrameworkElement root)
        {
            root.Loaded -= MainWindow_Loaded;
        }

        _ = DetectEncodersAsync();
        TryLoadCliArgument();
    }

    private async Task DetectEncodersAsync()
    {
        try
        {
            await _caps.DetectAsync().ConfigureAwait(true);
            RefreshEncoderUi();
        }
        catch (Exception ex)
        {
            GpuStatusTitle.Text = "GPU Acceleration: Unavailable";
            GpuStatusTitle.Foreground = (Brush)Application.Current.Resources["ResultLabelBrush"];
            GpuStatusIcon.Glyph = "\uE711";
            GpuStatusIcon.Foreground = (Brush)Application.Current.Resources["ResultLabelBrush"];
            GpuStatusName.Text = ex.Message;
        }
    }

    private void RefreshEncoderUi()
    {
        var gpu = _caps.Gpu;
        if (gpu.Available)
        {
            GpuStatusTitle.Text = "GPU Acceleration: Available";
            GpuStatusTitle.Foreground = (Brush)Application.Current.Resources["SuccessGreenBrush"];
            GpuStatusIcon.Glyph = "\uE73E";
            GpuStatusIcon.Foreground = (Brush)Application.Current.Resources["SuccessGreenBrush"];
            GpuStatusName.Text = string.IsNullOrWhiteSpace(gpu.Name)
                ? "Hardware encoder detected"
                : gpu.Name;
        }
        else
        {
            GpuStatusTitle.Text = "GPU Acceleration: Unavailable";
            GpuStatusTitle.Foreground = (Brush)Application.Current.Resources["ResultLabelBrush"];
            GpuStatusIcon.Glyph = "\uE711";
            GpuStatusIcon.Foreground = (Brush)Application.Current.Resources["ResultLabelBrush"];
            GpuStatusName.Text = string.IsNullOrWhiteSpace(gpu.Name)
                ? "Will use CPU encoders"
                : $"{gpu.Name} (no HW encoder)";
        }

        EncoderCombo.Items.Clear();
        foreach (var enc in _caps.Encoders)
        {
            EncoderCombo.Items.Add(new ComboBoxItem { Content = enc.Label, Tag = enc.Id });
        }

        if (EncoderCombo.Items.Count > 0)
        {
            EncoderCombo.SelectedIndex = 0;
        }

        FormatCombo.Items.Clear();
        foreach (var fmt in _caps.Formats)
        {
            FormatCombo.Items.Add(new ComboBoxItem { Content = fmt.Label, Tag = fmt.Id });
        }

        if (FormatCombo.Items.Count > 0)
        {
            FormatCombo.SelectedIndex = 0;
            UpdateFormatHelper();
        }

        UpdateGpuBadge();
    }

    private void EncoderCombo_SelectionChanged(object sender, SelectionChangedEventArgs e) =>
        UpdateGpuBadge();

    private void FormatCombo_SelectionChanged(object sender, SelectionChangedEventArgs e) =>
        UpdateFormatHelper();

    private void UpdateGpuBadge()
    {
        var enc = SelectedEncoder();
        GpuBadge.Visibility = enc?.IsGpu == true ? Visibility.Visible : Visibility.Collapsed;
    }

    private void UpdateFormatHelper()
    {
        var fmt = SelectedFormat();
        FormatHelperText.Text = fmt?.HelperText ?? string.Empty;
    }

    private EncoderOption? SelectedEncoder()
    {
        if (EncoderCombo.SelectedItem is ComboBoxItem item && item.Tag is string id)
        {
            return _caps.EncoderById(id);
        }

        return null;
    }

    private FormatOption? SelectedFormat()
    {
        if (FormatCombo.SelectedItem is ComboBoxItem item && item.Tag is string id)
        {
            return _caps.FormatById(id);
        }

        return null;
    }

    private void TryLoadCliArgument()
    {
        var args = Environment.GetCommandLineArgs();
        for (var i = 1; i < args.Length; i++)
        {
            var arg = args[i].Trim('"');
            if (File.Exists(arg) && IsSupportedVideo(arg))
            {
                _ = LoadVideoAsync(arg);
                return;
            }
        }
    }

    private static bool IsSupportedVideo(string path)
    {
        var ext = Path.GetExtension(path);
        return !string.IsNullOrEmpty(ext) && VideoExtensions.Contains(ext);
    }

    private async void SelectFileButton_Click(object sender, RoutedEventArgs e) =>
        await PickSourceFileAsync();

    private async void DropZone_Tapped(object sender, TappedRoutedEventArgs e) =>
        await PickSourceFileAsync();

    private async Task PickSourceFileAsync()
    {
        var picker = new FileOpenPicker();
        InitializePicker(picker);
        picker.ViewMode = PickerViewMode.Thumbnail;
        picker.SuggestedStartLocation = PickerLocationId.VideosLibrary;
        foreach (var ext in VideoExtensions)
        {
            picker.FileTypeFilter.Add(ext);
        }

        var file = await picker.PickSingleFileAsync();
        if (file is not null)
        {
            await LoadVideoAsync(file.Path);
        }
    }

    private void DropZone_DragOver(object sender, DragEventArgs e)
    {
        e.AcceptedOperation = DataPackageOperation.None;
        if (!e.DataView.Contains(StandardDataFormats.StorageItems))
        {
            return;
        }

        e.AcceptedOperation = DataPackageOperation.Copy;
    }

    private async void DropZone_Drop(object sender, DragEventArgs e)
    {
        if (!e.DataView.Contains(StandardDataFormats.StorageItems))
        {
            return;
        }

        var items = await e.DataView.GetStorageItemsAsync();
        if (items.Count == 0 || items[0] is not StorageFile file || !IsSupportedVideo(file.Path))
        {
            return;
        }

        await LoadVideoAsync(file.Path);
    }

    private void DropZone_PointerEntered(object sender, PointerRoutedEventArgs e) =>
        ApplyDropZoneVisualState(isHovered: true);

    private void DropZone_PointerExited(object sender, PointerRoutedEventArgs e) =>
        ApplyDropZoneVisualState(isHovered: false);

    private void ApplyDropZoneVisualState(bool isHovered)
    {
        if (_dropZoneHovered == isHovered)
        {
            return;
        }

        _dropZoneHovered = isHovered;
        DropZoneOutline.Stroke = isHovered ? DropZoneHoverStroke : DropZoneDefaultStroke;
        DropZoneOutline.Fill = isHovered ? DropZoneHoverFill : DropZoneDefaultFill;
    }

    private async Task LoadVideoAsync(string path)
    {
        try
        {
            _thumbCts?.Cancel();
            _mediaPlayer.Pause();
            _timelineThumbs.Clear();

            var meta = await _probe.ProbeAsync(path).ConfigureAwait(true);
            if (!meta.Valid)
            {
                await ShowMessageAsync("Could not read video metadata.");
                return;
            }

            _metadata = meta;
            FileNameLabel.Text = meta.FileName;
            TotalDurationLabel.Text = TimeFormat.FormatMs(meta.DurationMs);
            Timeline.SetDuration(meta.DurationMs);
            SyncTrimFields();
            UpdateDurationLabel();
            UpdateTimeLabels(0);
            SetControlsEnabled(true);

            _mediaPlayer.Source = MediaSource.CreateFromUri(new Uri(path));
            _mediaPlayer.PlaybackSession.Position = TimeSpan.Zero;

            _ = GenerateThumbnailsAsync(meta);
        }
        catch (Exception ex)
        {
            SetControlsEnabled(false);
            await ShowMessageAsync(ex.Message);
        }
    }

    private async Task GenerateThumbnailsAsync(VideoMetadata meta)
    {
        _thumbCts?.Cancel();
        _thumbCts = new CancellationTokenSource();
        var token = _thumbCts.Token;

        try
        {
            var count = Math.Clamp((int)(Timeline.ActualWidth / 90), 8, 24);
            var progress = new Progress<(int index, string pngPath)>(item =>
            {
                if (token.IsCancellationRequested)
                {
                    return;
                }

                while (_timelineThumbs.Count <= item.index)
                {
                    _timelineThumbs.Add(new BitmapImage());
                }

                _timelineThumbs[item.index] = new BitmapImage(new Uri(item.pngPath));
            });

            await _thumbnails.GenerateAsync(meta.Path, meta.DurationMs, count, progress, token)
                .ConfigureAwait(true);
        }
        catch (OperationCanceledException)
        {
            // Expected when loading another file.
        }
        catch
        {
            // Filmstrip is optional; preview still works.
        }
    }

    private void SetControlsEnabled(bool enabled)
    {
        PlayPauseButton.IsEnabled = enabled;
        ExportButton.IsEnabled = enabled;
        VolumeSlider.IsEnabled = enabled;
        Timeline.IsEnabled = enabled;
        StartEdit.IsEnabled = enabled;
        EndEdit.IsEnabled = enabled;
    }

    private void PlayPauseButton_Click(object sender, RoutedEventArgs e) => TogglePlayPause();

    private void PrevFrameButton_Click(object sender, RoutedEventArgs e) => StepFrame(-1);

    private void NextFrameButton_Click(object sender, RoutedEventArgs e) => StepFrame(1);

    private void JumpEndButton_Click(object sender, RoutedEventArgs e)
    {
        if (_metadata is null)
        {
            return;
        }

        SeekTo(Math.Max(0, _metadata.DurationMs - 1));
    }

    private void StepFrame(int direction)
    {
        if (_metadata is null)
        {
            return;
        }

        var fps = _metadata.FrameRate > 0 ? _metadata.FrameRate : 30;
        var stepMs = (long)Math.Max(1, Math.Round(1000.0 / fps));
        var pos = (long)_mediaPlayer.PlaybackSession.Position.TotalMilliseconds + direction * stepMs;
        SeekTo(Clamp(pos, 0, _metadata.DurationMs));
    }

    private void VolumeSlider_ValueChanged(object sender, RangeBaseValueChangedEventArgs e)
    {
        _mediaPlayer.Volume = e.NewValue / 100.0;
    }

    private void FullscreenButton_Click(object sender, RoutedEventArgs e) => ToggleFullscreen();

    private void Root_KeyDown(object sender, KeyRoutedEventArgs e)
    {
        if (e.Key == Windows.System.VirtualKey.Escape && _fullscreen)
        {
            ToggleFullscreen();
            e.Handled = true;
            return;
        }

        if (e.Key != Windows.System.VirtualKey.Space)
        {
            return;
        }

        if (FocusManager.GetFocusedElement(Content.XamlRoot) is TextBox)
        {
            return;
        }

        if (PlayPauseButton.IsEnabled)
        {
            TogglePlayPause();
            e.Handled = true;
        }
    }

    private void ToggleFullscreen()
    {
        if (_fullscreen)
        {
            AppWindow.SetPresenter(AppWindowPresenterKind.Default);
            _fullscreen = false;
        }
        else
        {
            AppWindow.SetPresenter(AppWindowPresenterKind.FullScreen);
            _fullscreen = true;
        }
    }

    private void TogglePlayPause()
    {
        if (_mediaPlayer.PlaybackSession.PlaybackState == MediaPlaybackState.Playing)
        {
            _mediaPlayer.Pause();
        }
        else
        {
            _mediaPlayer.Play();
        }
    }

    private void Timeline_TrimChanged(object? sender, EventArgs e)
    {
        SyncTrimFields();
        UpdateDurationLabel();
    }

    private void Timeline_SeekRequested(object? sender, long ms) => SeekTo(ms);

    private void Timeline_StartMarkerReleased(object? sender, EventArgs e)
    {
        SeekTo(Timeline.StartMs);
        _mediaPlayer.Play();
    }

    private void SeekTo(long ms)
    {
        _mediaPlayer.PlaybackSession.Position = TimeSpan.FromMilliseconds(ms);
        Timeline.PositionMs = ms;
        UpdateTimeLabels(ms);
    }

    private void PlaybackSession_PositionChanged(MediaPlaybackSession sender, object args)
    {
        var ms = (long)sender.Position.TotalMilliseconds;
        DispatcherQueue.TryEnqueue(() =>
        {
            Timeline.PositionMs = ms;
            UpdateTimeLabels(ms);
        });
    }

    private void PlaybackSession_PlaybackStateChanged(MediaPlaybackSession sender, object args)
    {
        DispatcherQueue.TryEnqueue(() =>
        {
            PlayPauseIcon.Glyph = sender.PlaybackState == MediaPlaybackState.Playing
                ? "\uE769"
                : "\uE768";
        });
    }

    private void SyncTrimFields()
    {
        _updatingFields = true;
        StartEdit.Text = TimeFormat.FormatMs(Timeline.StartMs);
        EndEdit.Text = TimeFormat.FormatMs(Timeline.EndMs);
        _updatingFields = false;
    }

    private void UpdateDurationLabel()
    {
        var dur = Math.Max(0, Timeline.EndMs - Timeline.StartMs);
        TrimDurationLabel.Text = $"Duration  {TimeFormat.FormatMs(dur)}";
    }

    private void UpdateTimeLabels(long positionMs)
    {
        var dur = _metadata?.DurationMs ?? 0;
        TimeReadout.Text = $"{TimeFormat.FormatMs(positionMs)} / {TimeFormat.FormatMs(dur)}";
    }

    private void StartEdit_LostFocus(object sender, RoutedEventArgs e) => ApplyStartEdit();

    private void EndEdit_LostFocus(object sender, RoutedEventArgs e) => ApplyEndEdit();

    private void TrimEdit_KeyDown(object sender, KeyRoutedEventArgs e)
    {
        if (e.Key != Windows.System.VirtualKey.Enter)
        {
            return;
        }

        if (ReferenceEquals(sender, StartEdit))
        {
            ApplyStartEdit();
        }
        else
        {
            ApplyEndEdit();
        }
    }

    private void ApplyStartEdit()
    {
        if (_updatingFields)
        {
            return;
        }

        if (!TimeFormat.TryParseToMs(StartEdit.Text, out var ms))
        {
            SyncTrimFields();
            return;
        }

        Timeline.SetTrimRange(Clamp(ms, 0, Timeline.EndMs), Timeline.EndMs);
        SyncTrimFields();
        UpdateDurationLabel();
    }

    private void ApplyEndEdit()
    {
        if (_updatingFields)
        {
            return;
        }

        if (!TimeFormat.TryParseToMs(EndEdit.Text, out var ms))
        {
            SyncTrimFields();
            return;
        }

        var max = _metadata?.DurationMs ?? Timeline.DurationMs;
        Timeline.SetTrimRange(Timeline.StartMs, Clamp(ms, Timeline.StartMs, max));
        SyncTrimFields();
        UpdateDurationLabel();
    }

    private void StartUp_Click(object sender, RoutedEventArgs e) => StepStart(1);

    private void StartDown_Click(object sender, RoutedEventArgs e) => StepStart(-1);

    private void EndUp_Click(object sender, RoutedEventArgs e) => StepEnd(1);

    private void EndDown_Click(object sender, RoutedEventArgs e) => StepEnd(-1);

    private void StepStart(int direction)
    {
        var ms = Clamp(Timeline.StartMs + direction * 100, 0, Timeline.EndMs);
        Timeline.SetTrimRange(ms, Timeline.EndMs);
        SyncTrimFields();
        UpdateDurationLabel();
    }

    private void StepEnd(int direction)
    {
        var max = _metadata?.DurationMs ?? Timeline.DurationMs;
        var ms = Clamp(Timeline.EndMs + direction * 100, Timeline.StartMs, max);
        Timeline.SetTrimRange(Timeline.StartMs, ms);
        SyncTrimFields();
        UpdateDurationLabel();
    }

    private async void ExportButton_Click(object sender, RoutedEventArgs e)
    {
        if (_metadata is null)
        {
            return;
        }

        if (Timeline.EndMs <= Timeline.StartMs)
        {
            await ShowMessageAsync("End time must be greater than Start time.");
            return;
        }

        var format = SelectedFormat();
        var encoder = SelectedEncoder();
        if (format is null || encoder is null)
        {
            await ShowMessageAsync("Select an encoder and format first.");
            return;
        }

        var picker = new FileSavePicker();
        InitializePicker(picker);
        picker.SuggestedStartLocation = PickerLocationId.VideosLibrary;
        picker.SuggestedFileName = $"{Path.GetFileNameWithoutExtension(_metadata.FileName)}_trim";
        picker.FileTypeChoices.Add(format.Label, [$".{format.DefaultExtension}"]);

        var file = await picker.PickSaveFileAsync();
        if (file is null)
        {
            return;
        }

        ExportButton.IsEnabled = false;
        _exportCts?.Cancel();
        _exportCts = new CancellationTokenSource();

        _progressBar = new ProgressBar { Minimum = 0, Maximum = 100, Height = 8 };
        _progressStatus = new TextBlock
        {
            Text = "Exporting...",
            Style = (Style)Application.Current.Resources["BodyTextStyle"],
            TextWrapping = TextWrapping.WrapWholeWords,
        };
        var panel = new StackPanel { Spacing = 12 };
        panel.Children.Add(_progressStatus);
        panel.Children.Add(_progressBar);

        _progressDialog = new ContentDialog
        {
            XamlRoot = Content.XamlRoot,
            Title = "Trim & Export",
            Content = panel,
            CloseButtonText = "Cancel",
            RequestedTheme = ElementTheme.Light,
        };

        var dialogTask = _progressDialog.ShowAsync().AsTask();
        var exportTask = RunExportAsync(file.Path, encoder, format, _exportCts.Token);

        var completed = await Task.WhenAny(dialogTask, exportTask).ConfigureAwait(true);
        if (ReferenceEquals(completed, dialogTask))
        {
            _exportCts.Cancel();
            try
            {
                await exportTask.ConfigureAwait(true);
            }
            catch
            {
                // Cancelled or failed after dismiss.
            }
        }
        else
        {
            _progressDialog.Hide();
            await dialogTask.ConfigureAwait(true);
            var (ok, message) = await exportTask.ConfigureAwait(true);
            await ShowMessageAsync(message, ok ? "Trimmi" : "Export failed");
        }

        ExportButton.IsEnabled = true;
        _progressDialog = null;
        _progressBar = null;
        _progressStatus = null;
    }

    private async Task<(bool Ok, string Message)> RunExportAsync(
        string outputPath,
        EncoderOption encoder,
        FormatOption format,
        CancellationToken cancellationToken)
    {
        try
        {
            var request = new ExportRequest
            {
                InputPath = _metadata!.Path,
                OutputPath = outputPath,
                StartMs = Timeline.StartMs,
                EndMs = Timeline.EndMs,
                Encoder = encoder,
                Format = format,
            };

            var progress = new Progress<ExportProgress>(p =>
            {
                DispatcherQueue.TryEnqueue(() =>
                {
                    if (_progressBar is not null)
                    {
                        _progressBar.Value = Math.Clamp(p.Percent, 0, 100);
                    }

                    if (_progressStatus is not null && !string.IsNullOrWhiteSpace(p.Status))
                    {
                        _progressStatus.Text = p.Status;
                    }
                });
            });

            await _exporter.ExportAsync(request, progress, cancellationToken).ConfigureAwait(true);
            return (true, $"Exported to:\n{outputPath}");
        }
        catch (OperationCanceledException)
        {
            return (false, "Export cancelled.");
        }
        catch (Exception ex)
        {
            return (false, ex.Message);
        }
    }

    private async Task ShowMessageAsync(string message, string title = "Trimmi")
    {
        var dialog = new ContentDialog
        {
            XamlRoot = Content.XamlRoot,
            Title = title,
            Content = message,
            CloseButtonText = "OK",
            RequestedTheme = ElementTheme.Light,
        };
        await dialog.ShowAsync();
    }

    private void InitializePicker(object picker)
    {
        var hwnd = WindowNative.GetWindowHandle(this);
        InitializeWithWindow.Initialize(picker, hwnd);
    }

    private static long Clamp(long value, long min, long max) =>
        Math.Min(Math.Max(value, min), max);
}
