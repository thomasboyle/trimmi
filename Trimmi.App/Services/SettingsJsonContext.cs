using System.Text.Json.Serialization;
using Trimmi.Core.Models;

namespace Trimmi_App.Services;

[JsonSourceGenerationOptions(WriteIndented = true)]
[JsonSerializable(typeof(AppSettings))]
internal sealed partial class SettingsJsonContext : JsonSerializerContext
{
}
