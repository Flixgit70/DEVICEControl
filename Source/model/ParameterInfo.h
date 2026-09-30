#pragma once
#include <JuceHeader.h>
#include <cmath>

namespace er
{
enum class ParameterKind { unknown, integer, real, stringValue, booleanValue, trigger, enumeration, octets };

struct ParameterInfo
{
    // GUI-facing path. In multi-provider mode this is provider-qualified.
    juce::String path;
    // Provider metadata is filled by ProviderManager. Individual provider backends
    // keeps these empty and continues to work only with numeric Ember paths.
    juce::String providerId;
    juce::String providerName;
    juce::String localPath;
    juce::String identifier;
    juce::String description;
    juce::String treePath;
    ParameterKind kind = ParameterKind::unknown;
    juce::var value;
    juce::var minimum;
    juce::var maximum;
    double factor = 1.0;
    bool writable = false;
    bool online = true;
    juce::StringArray enumEntries;

    juce::String displayName() const
    {
        if (identifier.isNotEmpty()) return identifier;
        if (description.isNotEmpty()) return description;
        return path;
    }

    // Human-readable complete Ember branch for control Label 1.
    // EmberClient stores treePath as "Node / Child / Parameter"; controls and
    // CSV use the compact slash form requested by the UI: Node/Child/Parameter.
    juce::String fullTreeLabel() const
    {
        const auto localLabel = treePath.isNotEmpty() ? treePath.replace(" / ", "/") : displayName();
        if (providerName.isNotEmpty())
            return providerName + "/" + localLabel;
        return localLabel;
    }

    // Human-readable current value for compact status/UI displays.
    // Numeric values respect Ember's factor in the same way as the encoder tile.
    juce::String formattedValue() const
    {
        if (value.isVoid())
            return "-";

        switch (kind)
        {
            case ParameterKind::booleanValue:
                return static_cast<bool>(value) ? "true" : "false";

            case ParameterKind::enumeration:
            {
                const int raw = (int) (juce::int64) value;
                for (const auto& entry : enumEntries)
                {
                    if (entry.upToFirstOccurrenceOf(":", false, false).getIntValue() == raw)
                    {
                        const auto label = entry.fromFirstOccurrenceOf(":", false, false).trim();
                        return label.isNotEmpty() ? label : juce::String(raw);
                    }
                }
                return juce::String(raw);
            }

            case ParameterKind::integer:
            case ParameterKind::real:
            {
                const double safeFactor = factor == 0.0 ? 1.0 : factor;
                const double display = value.toString().getDoubleValue() / safeFactor;
                if (kind == ParameterKind::integer && safeFactor == 1.0)
                    return juce::String((juce::int64) std::llround(display));

                auto text = juce::String(display, 3);
                if (text.containsChar('.'))
                    text = text.trimCharactersAtEnd("0").trimCharactersAtEnd(".");
                return text.isEmpty() || text == "-" ? juce::String("0") : text;
            }

            default:
                return value.toString();
        }
    }

    static juce::String kindToString(ParameterKind k)
    {
        switch (k)
        {
            case ParameterKind::integer: return "integer";
            case ParameterKind::real: return "real";
            case ParameterKind::stringValue: return "string";
            case ParameterKind::booleanValue: return "boolean";
            case ParameterKind::trigger: return "trigger";
            case ParameterKind::enumeration: return "enum";
            case ParameterKind::octets: return "octets";
            default: return "unknown";
        }
    }
};
}
