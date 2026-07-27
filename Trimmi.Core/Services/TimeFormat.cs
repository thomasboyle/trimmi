using System.Globalization;
using System.Text.RegularExpressions;

namespace Trimmi.Core.Services;

public static partial class TimeFormat
{
    [GeneratedRegex(@"^(?:(\d+):)?(\d{1,2}):(\d{1,2})(?:\.(\d{1,3}))?$", RegexOptions.CultureInvariant)]
    private static partial Regex TimeRegex();

    public static string FormatMs(long ms, bool includeMillis = true)
    {
        if (ms < 0)
            ms = 0;

        var hours = ms / 3_600_000;
        var minutes = (ms % 3_600_000) / 60_000;
        var seconds = (ms % 60_000) / 1000;
        var millis = ms % 1000;

        if (includeMillis)
            return $"{hours:D2}:{minutes:D2}:{seconds:D2}.{millis:D3}";

        return $"{hours:D2}:{minutes:D2}:{seconds:D2}";
    }

    public static bool TryParseToMs(string text, out long ms)
    {
        ms = 0;
        var trimmed = text.Trim();
        var match = TimeRegex().Match(trimmed);
        if (!match.Success)
            return false;

        var hours = match.Groups[1].Success
            ? long.Parse(match.Groups[1].Value, CultureInfo.InvariantCulture)
            : 0;
        var minutes = long.Parse(match.Groups[2].Value, CultureInfo.InvariantCulture);
        var seconds = long.Parse(match.Groups[3].Value, CultureInfo.InvariantCulture);

        var millisStr = match.Groups[4].Success ? match.Groups[4].Value : "0";
        while (millisStr.Length < 3)
            millisStr += "0";
        if (millisStr.Length > 3)
            millisStr = millisStr[..3];

        var millis = long.Parse(millisStr, CultureInfo.InvariantCulture);
        ms = hours * 3_600_000 + minutes * 60_000 + seconds * 1000 + millis;
        return true;
    }
}
