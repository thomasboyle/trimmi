using System.Collections.ObjectModel;
using System.Collections.Specialized;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Microsoft.UI.Xaml.Shapes;
using Trimmi.Core.Services;
using Windows.Foundation;

namespace Trimmi_App.Views;

public sealed partial class TimelineControl : UserControl
{
    public event EventHandler? TrimChanged;
    public event EventHandler<long>? SeekRequested;
    public event EventHandler? StartMarkerReleased;

    public static readonly DependencyProperty DurationMsProperty =
        DependencyProperty.Register(nameof(DurationMs), typeof(long), typeof(TimelineControl),
            new PropertyMetadata(0L, OnLayoutPropertyChanged));

    public static readonly DependencyProperty PositionMsProperty =
        DependencyProperty.Register(nameof(PositionMs), typeof(long), typeof(TimelineControl),
            new PropertyMetadata(0L, OnLayoutPropertyChanged));

    public static readonly DependencyProperty StartMsProperty =
        DependencyProperty.Register(nameof(StartMs), typeof(long), typeof(TimelineControl),
            new PropertyMetadata(0L, OnLayoutPropertyChanged));

    public static readonly DependencyProperty EndMsProperty =
        DependencyProperty.Register(nameof(EndMs), typeof(long), typeof(TimelineControl),
            new PropertyMetadata(0L, OnLayoutPropertyChanged));

    public static readonly DependencyProperty ThumbnailsProperty =
        DependencyProperty.Register(nameof(Thumbnails), typeof(ObservableCollection<BitmapImage>), typeof(TimelineControl),
            new PropertyMetadata(null, OnThumbnailsChanged));

    private enum DragTarget
    {
        None,
        Start,
        End,
        Playhead,
        Selection,
    }

    private DragTarget _drag = DragTarget.None;
    private long _dragOffsetMs;
    private bool _suppressEvents;

    public long DurationMs
    {
        get => (long)GetValue(DurationMsProperty);
        set => SetValue(DurationMsProperty, value);
    }

    public long PositionMs
    {
        get => (long)GetValue(PositionMsProperty);
        set => SetValue(PositionMsProperty, value);
    }

    public long StartMs
    {
        get => (long)GetValue(StartMsProperty);
        set => SetValue(StartMsProperty, value);
    }

    public long EndMs
    {
        get => (long)GetValue(EndMsProperty);
        set => SetValue(EndMsProperty, value);
    }

    public ObservableCollection<BitmapImage>? Thumbnails
    {
        get => (ObservableCollection<BitmapImage>?)GetValue(ThumbnailsProperty);
        set => SetValue(ThumbnailsProperty, value);
    }

    public TimelineControl()
    {
        InitializeComponent();
        Thumbnails = [];
        Loaded += (_, _) => RefreshLayout();
    }

    public void SetDuration(long durationMs)
    {
        _suppressEvents = true;
        DurationMs = Math.Max(0, durationMs);
        StartMs = 0;
        EndMs = DurationMs;
        PositionMs = 0;
        _suppressEvents = false;
        RefreshLayout();
    }

    public void SetTrimRange(long startMs, long endMs)
    {
        _suppressEvents = true;
        StartMs = Clamp(startMs, 0, DurationMs);
        EndMs = Clamp(endMs, StartMs, DurationMs);
        _suppressEvents = false;
        RefreshLayout();
        TrimChanged?.Invoke(this, EventArgs.Empty);
    }

    private static void OnLayoutPropertyChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
    {
        if (d is TimelineControl control)
        {
            control.RefreshLayout();
        }
    }

    private static void OnThumbnailsChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
    {
        if (d is not TimelineControl control)
        {
            return;
        }

        if (e.OldValue is ObservableCollection<BitmapImage> oldItems)
        {
            oldItems.CollectionChanged -= control.Thumbnails_CollectionChanged;
        }

        if (e.NewValue is ObservableCollection<BitmapImage> newItems)
        {
            newItems.CollectionChanged += control.Thumbnails_CollectionChanged;
            control.ThumbnailStrip.ItemsSource = newItems;
        }
        else
        {
            control.ThumbnailStrip.ItemsSource = null;
        }

        control.RefreshLayout();
    }

    private void Thumbnails_CollectionChanged(object? sender, NotifyCollectionChangedEventArgs e) =>
        DispatcherQueue.TryEnqueue(RefreshThumbnailWidths);

    private void FilmstripHost_SizeChanged(object sender, SizeChangedEventArgs e) => RefreshLayout();

    private void RefreshLayout()
    {
        var width = FilmstripHost.ActualWidth;
        if (width <= 0)
        {
            BuildRuler();
            DimLeft.Width = 0;
            DimRight.Width = 0;
            DimRight.Margin = new Thickness(0);
            SelectionOverlay.Width = 0;
            SelectionOverlay.Margin = new Thickness(0);
            Playhead.Margin = new Thickness(0, -4, 0, -2);
            StartThumb.Margin = new Thickness(0, -4, 0, -4);
            EndThumb.Margin = new Thickness(0, -4, 0, -4);
            HandleLabelCanvas.Children.Clear();
            return;
        }

        // Empty / no-duration: show full-range markers so both ends are visible.
        if (DurationMs <= 0)
        {
            BuildRuler();
            DimLeft.Width = 0;
            DimRight.Width = 0;
            DimRight.Margin = new Thickness(0);
            SelectionOverlay.Margin = new Thickness(0);
            SelectionOverlay.Width = width;
            Playhead.Margin = new Thickness(0, -4, 0, -2);
            PlaceThumbs(0, width, width);
            HandleLabelCanvas.Children.Clear();
            AddHandleLabel("Start", 0, width, isStart: true);
            AddHandleLabel("End", width, width, isStart: false);
            return;
        }

        var startX = MsToX(StartMs, width);
        var endX = MsToX(EndMs, width);
        var playX = MsToX(PositionMs, width);

        DimLeft.Width = Math.Max(0, startX);
        DimRight.Margin = new Thickness(endX, 0, 0, 0);
        DimRight.Width = Math.Max(0, width - endX);

        SelectionOverlay.Margin = new Thickness(startX, 0, 0, 0);
        SelectionOverlay.Width = Math.Max(1, endX - startX);

        Playhead.Margin = new Thickness(playX - 1, -4, 0, -2);
        PlaceThumbs(startX, endX, width);

        BuildRuler();
        BuildHandleLabels(startX, endX, width);
        RefreshThumbnailWidths();
    }

    private const double TrackInset = 12;
    private const double ThumbHalf = 9;
    private const double ThumbWidth = ThumbHalf * 2;

    private void PlaceThumbs(double startX, double endX, double width)
    {
        // Thumbs sit in a full-width overlay; filmstrip is inset by TrackInset.
        var startThumbX = TrackInset + startX - ThumbHalf;
        var endThumbX = TrackInset + endX - ThumbHalf;
        if (Math.Abs(endThumbX - startThumbX) < ThumbWidth)
        {
            startThumbX = TrackInset - ThumbHalf;
            endThumbX = TrackInset + width - ThumbHalf;
        }

        StartThumb.Margin = new Thickness(startThumbX, -4, 0, -4);
        EndThumb.Margin = new Thickness(endThumbX, -4, 0, -4);
    }

    private void RefreshThumbnailWidths()
    {
        var count = Thumbnails?.Count ?? 0;
        if (count <= 0 || FilmstripHost.ActualWidth <= 0)
        {
            return;
        }

        var cell = FilmstripHost.ActualWidth / count;
        foreach (var item in ThumbnailStrip.Items)
        {
            if (ThumbnailStrip.ContainerFromItem(item) is FrameworkElement element)
            {
                element.Width = cell;
            }
        }
    }

    private void BuildRuler()
    {
        RulerCanvas.Children.Clear();
        var width = FilmstripHost.ActualWidth;
        if (width <= 0 || DurationMs <= 0)
        {
            return;
        }

        var tickCount = Math.Clamp((int)(ActualWidth / 140), 2, 8);
        for (var i = 0; i <= tickCount; i++)
        {
            var ms = DurationMs * i / tickCount;
            var x = MsToX(ms, width);
            RulerCanvas.Children.Add(new Line
            {
                X1 = x,
                Y1 = 17,
                X2 = x,
                Y2 = 22,
                Stroke = new SolidColorBrush(Windows.UI.Color.FromArgb(0xFF, 0xA7, 0xA2, 0x8B)),
                StrokeThickness = 1,
            });

            var label = new TextBlock
            {
                Text = TimeFormat.FormatMs(ms, includeMillis: true),
                FontSize = 11,
                Foreground = new SolidColorBrush(Windows.UI.Color.FromArgb(0xBF, 0x2A, 0x32, 0x20)),
            };
            label.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
            var labelWidth = label.DesiredSize.Width;
            var left = i == 0
                ? x
                : i == tickCount
                    ? x - labelWidth
                    : x - labelWidth / 2;
            Canvas.SetLeft(label, left);
            Canvas.SetTop(label, 2);
            RulerCanvas.Children.Add(label);
        }
    }

    private void BuildHandleLabels(double startX, double endX, double width)
    {
        HandleLabelCanvas.Children.Clear();
        AddHandleLabel("Start", startX, width, isStart: true);
        AddHandleLabel("End", endX, width, isStart: false);
    }

    private void AddHandleLabel(string text, double x, double width, bool isStart)
    {
        var label = new TextBlock
        {
            Text = text,
            FontSize = 11,
            FontWeight = Microsoft.UI.Text.FontWeights.Bold,
            Foreground = new SolidColorBrush(Windows.UI.Color.FromArgb(0xFF, 0x2A, 0x32, 0x20)),
        };
        label.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
        var labelWidth = label.DesiredSize.Width;
        double left;
        if (isStart && x < labelWidth)
            left = 0;
        else if (!isStart && x > width - labelWidth)
            left = Math.Max(0, width - labelWidth);
        else
            left = Math.Clamp(x - labelWidth / 2, 0, Math.Max(0, width - labelWidth));

        Canvas.SetLeft(label, left);
        Canvas.SetTop(label, 2);
        HandleLabelCanvas.Children.Add(label);
    }

    private void StartThumb_PointerPressed(object sender, PointerRoutedEventArgs e)
    {
        if (DurationMs <= 0)
        {
            return;
        }

        _drag = DragTarget.Start;
        StartThumb.CapturePointer(e.Pointer);
        e.Handled = true;
    }

    private void EndThumb_PointerPressed(object sender, PointerRoutedEventArgs e)
    {
        if (DurationMs <= 0)
        {
            return;
        }

        _drag = DragTarget.End;
        EndThumb.CapturePointer(e.Pointer);
        e.Handled = true;
    }

    private void Filmstrip_PointerPressed(object sender, PointerRoutedEventArgs e)
    {
        if (DurationMs <= 0 || FilmstripHost.ActualWidth <= 0)
        {
            return;
        }

        var point = e.GetCurrentPoint(FilmstripHost).Position;
        var ms = XToMs(point.X, FilmstripHost.ActualWidth);
        var startX = MsToX(StartMs, FilmstripHost.ActualWidth);
        var endX = MsToX(EndMs, FilmstripHost.ActualWidth);

        if (point.X >= startX - 8 && point.X <= startX + 8)
        {
            _drag = DragTarget.Start;
        }
        else if (point.X >= endX - 8 && point.X <= endX + 8)
        {
            _drag = DragTarget.End;
        }
        else if (point.X >= startX && point.X <= endX)
        {
            _drag = DragTarget.Selection;
            _dragOffsetMs = ms - StartMs;
        }
        else
        {
            _drag = DragTarget.Playhead;
            PositionMs = ms;
            SeekRequested?.Invoke(this, ms);
        }

        FilmstripHost.CapturePointer(e.Pointer);
        e.Handled = true;
    }

    private void Filmstrip_PointerMoved(object sender, PointerRoutedEventArgs e)
    {
        if (_drag == DragTarget.None || DurationMs <= 0 || FilmstripHost.ActualWidth <= 0)
        {
            return;
        }

        var x = e.GetCurrentPoint(FilmstripHost).Position.X;
        var ms = XToMs(x, FilmstripHost.ActualWidth);

        switch (_drag)
        {
            case DragTarget.Start:
                StartMs = Clamp(ms, 0, EndMs);
                EmitTrim();
                break;
            case DragTarget.End:
                EndMs = Clamp(ms, StartMs, DurationMs);
                EmitTrim();
                break;
            case DragTarget.Playhead:
                PositionMs = ms;
                SeekRequested?.Invoke(this, ms);
                break;
            case DragTarget.Selection:
            {
                var span = EndMs - StartMs;
                var newStart = Clamp(ms - _dragOffsetMs, 0, Math.Max(0, DurationMs - span));
                StartMs = newStart;
                EndMs = newStart + span;
                EmitTrim();
                break;
            }
        }

        RefreshLayout();
        e.Handled = true;
    }

    private void Filmstrip_PointerReleased(object sender, PointerRoutedEventArgs e)
    {
        if (_drag == DragTarget.None)
        {
            return;
        }

        var released = _drag;
        _drag = DragTarget.None;
        FilmstripHost.ReleasePointerCaptures();
        StartThumb.ReleasePointerCaptures();
        EndThumb.ReleasePointerCaptures();
        e.Handled = true;

        if (released == DragTarget.Start)
        {
            PositionMs = StartMs;
            SeekRequested?.Invoke(this, StartMs);
            StartMarkerReleased?.Invoke(this, EventArgs.Empty);
        }
    }

    private void EmitTrim()
    {
        if (!_suppressEvents)
        {
            TrimChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    private double MsToX(long ms, double width)
    {
        if (DurationMs <= 0 || width <= 0)
        {
            return 0;
        }

        return Math.Clamp(ms / (double)DurationMs, 0, 1) * width;
    }

    private long XToMs(double x, double width)
    {
        if (DurationMs <= 0 || width <= 0)
        {
            return 0;
        }

        return (long)(Math.Clamp(x / width, 0, 1) * DurationMs);
    }

    private static long Clamp(long value, long min, long max) =>
        Math.Min(Math.Max(value, min), max);
}
