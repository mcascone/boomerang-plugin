# JUCE Tips and Tricks

A comprehensive reference for JUCE audio plugin development, compiled from [melatonin.dev](https://melatonin.dev/blog/big-list-of-juce-tips-and-tricks/) and adapted for this project.

---

## Getting Started

### The JUCE Forum is Key

The JUCE docs can be light on examples and gotchas. The community hasn't historically been able to contribute to docs, so they aren't always beginner-friendly.

- **Use the [JUCE Forum](http://forum.juce.com/)** - it has community examples of pretty much everything and 95% of problems you'll encounter
- **Check the [tutorials](https://juce.com/learn/tutorials)** - they describe mental models and concepts the docs gloss over

---

## Development Workflow

### Build and Run the Standalone to Save Time

The standalone is the fastest plugin format to iterate on because it doesn't need to start within a host.

- In CMake, add `Standalone` to `FORMATS` (see [pamplejuce](http://github.com/sudara/pamplejuce))
- Building standalone doesn't require any plugin APIs
- **Bonus tip:** To ensure audio/midi device info and plugin state are saved, gracefully quit the standalone (close the window, don't hit stop)

### Save Your AudioPluginHost Setup

If using JUCE's AudioPluginHost as a test host:
- `File > Save` settings as a `.filtergraph`
- AudioPluginHost will autoload the last saved "patch" on next launch
- **Bonus:** Add EQ and metering you use regularly to the chain

### Make Sure You're Running the Latest Version

Before debugging why a fix isn't showing up, verify your plugin was actually updated:

1. Ensure `COPY_PLUGIN_AFTER_BUILD TRUE` is set in `juce_add_plugin`
2. Don't have your DAW open with the plugin while building (copy may fail)
3. Check permissions on target folder (especially Windows)

**Debug tip:** Add a timestamp label to your UI:
```cpp
label.setText (__DATE__ " " __TIME__ " " CMAKE_BUILD_TYPE, dontSendNotification);
```

Expose build type in CMakeLists.txt:
```cmake
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
```

---

## Debugging

### `DBG` Will Add Dropouts and Glitches in Debug

`DBG` works with strings and **allocates memory**. You'll hear issues if you fire too many in the audio thread.

### `DBG` in Your DAW

To see `DBG` output in a DAW context:
- **Xcode:** Enable "Debug Executable" checkbox
- **CLion:** Add a custom configuration pointing to your DAW

⚠️ **Apple Silicon note:** Debugging in DAW requires disabling SIP and manually attaching to `AUHostingServiceXPC_arrow` process.

### Use a Better Macro for Variable Debugging

```cpp
/* D is a helper to print variables
 * D(myFloat);          // myFloat: 1.89
 * D(myFloat, myBool)   // myFloat: 1.89, myBool: 0
 */

#if (DEBUG)
    #define D(...) makePrintable (#__VA_ARGS__, __VA_ARGS__)

    template <typename Arg1>
    void makePrintable (const char* name, Arg1&& arg1)
    {
        std::cout << name << ": " << arg1 << std::endl;
    }

    template <typename Arg1, typename... Args>
    void makePrintable (const char* names, Arg1&& arg1, Args&&... args)
    {
        const char* comma = std::strchr (names + 1, ',');
        std::cout.write (names, comma - names) << ": " << arg1;
        makePrintable (comma, args...);
    }
#else
    #define D(...) // strings allocate, so no-op in Release
#endif
```

### Better JUCE Type Debugging in Your IDE

- [juce-toys](https://github.com/jcredland/juce-toys) - lldb script and natvis config for `juce::String`, `juce::Identifier`, `juce::var`, `juce::ValueTree`
- [Audio sparklines](https://melatonin.dev/blog/audio-sparklines/) - visualize AudioBlocks in your IDE

### When You Hit the JUCE Leak Detector

```
*** Leaked objects detected: 18 instance(s) of class AudioProcessorParameterWithID
JUCE Assertion failure in juce_LeakedObjectDetector.h:92
```

**Keep hitting `continue`** in your debugger - often there are more object types leaking. The first leaked objects might be members of later leaked objects.

---

## Platform-Specific Issues

### AU Not Showing Up on macOS?

AU plugins must be in `/Library/Audio/Plugins` or `~/Library/Audio/Plugins` subfolders (`Components`, `VST3`, etc).

If not scanning, kick the AU registrar:
```bash
killall -9 AudioComponentRegistrar
```

**Bonus tip:** Set version number to 0 for debug builds and your plugin will auto-scan every time.

### AUv3 Won't Register Until You Run the Standalone

The AUv3 gets registered when you use Finder to launch the standalone app containing the AUv3 appex bundle (double-click in Finder).

### TargetConditionals.h Not Found on macOS

After updating Xcode on a CMake project:
```bash
rm -rf build/
```

### Statically Link the Windows Runtime

Dynamic linking can cause problems. Ensure static in CMake:
```cmake
if (WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "")
endif ()
```

Inspect dynamic dependencies:
```cmd
dumpbin /dependents <your binary path>
```

---

## UI Development

### Assume UI Will Be the Most Time-Consuming

The hard part of building a real plugin is UI. This is where most invested time goes.

**Tools:**
- [Melatonin Inspector](https://github.com/sudara/melatonin_inspector) - inspect component hierarchy and positioning
- [pluginGUIMagic](https://foleysfinest.com/developer/pluginguimagic/) - handles all UI for you

### Pixels in JUCE Are Logical

Heights and widths in JUCE are [logical pixels](https://blog.specctr.com/pixels-physical-vs-logical-c84710199d62) - device and resolution independent. Don't confuse with physical screen pixels.

### Draw with Floats, Use Int for Component Bounds

- Components can have sub-pixel position/size when drawn to scale or transformed
- Component bounds are always specified as integer-perfect pixels
- Someday we'll get float component bounds in JUCE!

### Enable Repaint Debugging

Flashes repainted areas with random colors - good first line of defense for jank/glitch problems.

```cmake
target_compile_definitions(YourTarget PRIVATE JUCE_ENABLE_REPAINT_DEBUGGING=1)
```

On macOS, also try [Quartz Debug](https://developer.apple.com/library/archive/documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/Testing/Testing.html).

### Edit Any Value Live with `JUCE_LIVE_CONSTANT`

```cpp
shadows.setRadius (JUCE_LIVE_CONSTANT(blurRadius));
shadows.setColor (colors[i].withAlpha (JUCE_LIVE_CONSTANT (0.7f)));
```

On recompile, you get a UI to live-edit values.

### For Best Performance, Treat Drawing Like the Audio Thread

Avoid allocation in paint calls when possible:
- Use `juce::Path`, `juce::Image`, `juce::ColourGradient` as member variables
- Allocate once on open, not per-paint call

For deep component performance inspection: [melatonin_perfetto](http://github.com/sudara/melatonin_perfetto)

---

## Component Architecture

### LookAndFeels Come Before Components in Member List

Order matters on destruction - members are deleted in reverse order.

```cpp
class MyComponent : public juce::Component
{
private:
    CustomLookAndFeel customLookAndFeel;  // Deleted LAST
    MyOtherComponent myOtherComponent;     // Deleted FIRST (uses look and feel)
};
```

### If Your Plugin UI Asserts on Re-open, Check Your Listeners

Look for all `addListener` and `addChangeListener` calls and ensure you call `removeListener` in destructors.

### Get Comfy Building Your Own Widgets

It often takes less time to build exactly what you need than to contort stock widgets.

- JUCE widgets have legacy cruft and limited maintenance
- Can't do shadows properly (but [Melatonin Blur](http://github.com/sudara/melatonin_blur) can)
- Wasn't built for rounded corners

**Start simple:** If you just want to show text without editing, don't use a `Label` - use `g.drawText` in `paint`.

**Exception:** `TextEditor` has sophisticated multi-line text rendering and selection logic - not fun to reinvent.

### Don't Write LookAndFeels for Custom Widgets

LookAndFeels exist so JUCE can offer customizable widgets. But you aren't making a framework.

Keep all drawing in `paint`. You'll thank yourself later.

---

## Audio Thread Safety

### `parameterChanged` Can Happen on the Audio Thread

⚠️ **Important:** Both `parameterChanged` and `parameterValueChanged` can be called from any thread, including the message thread or audio thread. It depends on the DAW.

**Do:**
- Only trivial operations in parameter callbacks
- Treat them like you're on the audio thread

**Avoid:**
- Calling `repaint`
- Heavy lifting for audio
- `AsyncUpdater` and `Component::postCommandMessage` (not lock-free)

**Pattern using Timer:**
```cpp
class MyComponent : public juce::Component, juce::Timer, 
                    juce::AudioProcessorParameter::Listener
{
public:
    MyComponent()
    {
        startTimerHz (60);
    }

    void timerCallback() override
    {
        if (recalculate)
            repaint();
    }

    void parameterValueChanged (int, float) override
    {
        if (!recalculate)
            recalculate = true;
    }
    
private:
    std::atomic<bool> recalculate = false;
};
```

On JUCE 7.0.6+, use `VBlankAttachment` instead of timer for next-frame repaints.

### Beware Dereferencing Tons of Atomics Per Buffer

Calling `gain->load()` per-sample in a tight loop in `processBlock` incurs a performance penalty.

⚠️ This only matters if you're dereferencing atomics hundreds of times in an inner loop.

### You Can Suspend Audio Processing

```cpp
suspendProcessing(true);
```

Use for heavy lifting like preset switching or convolution reallocation - callbacks will return silence.

### Avoid Static Memory and Singletons in Plugins

`static` memory creates issues with multiple plugin instances - they could be in same process or different processes depending on DAW.

- Singletons are static memory too
- `thread_local` won't help - hosts may run entire channel chains on one thread

**Solution:** Manually pass state/data down through components. Organize hierarchy so classes get minimum state needed.

---

## Testing & Validation

### Your Plugin Isn't Ready Until It Passes pluginval

Be prepared to fix 5 bugs you didn't know you had. See [pluginval guide](https://melatonin.dev/blog/pluginval-is-a-plugin-devs-best-friend/).

---

## Tools & IDE Setup

### Use CLion for Cross-Platform Consistency

1. Consistent dev experience on macOS and Windows
2. Syncs IDE settings between computers/platforms
3. Built-in CMake support - picks up CMakeLists.txt changes
4. Built-in [Catch2 support](https://melatonin.dev/blog/when-to-write-tests-for-dsp-code/)

### Use clang-format

See [pamplejuce example](http://github.com/sudara/pamplejuce) and [Adam Wilson's post](https://forum.juce.com/t/automatic-juce-like-code-formatting-with-clang-format/31624/20).

### New to Xcode? Turn on the Console

`View > Debug Area > Activate Console`

---

## Useful Modules & Libraries

| Module | Purpose |
|--------|---------|
| [Melatonin Inspector](https://github.com/sudara/melatonin_inspector) | Inspect component hierarchy like Web Inspector |
| [Melatonin Blur](http://github.com/sudara/melatonin_blur) | Proper shadow support |
| [melatonin_perfetto](http://github.com/sudara/melatonin_perfetto) | Component performance profiling |
| [juce-toys](https://github.com/jcredland/juce-toys) | Better JUCE type debugging |
| [pluginGUIMagic](https://foleysfinest.com/developer/pluginguimagic/) | Automatic UI generation |

---

## Quick Reference

| Topic | Key Point |
|-------|-----------|
| `DBG` | Allocates memory - causes dropouts in audio thread |
| Standalone | Fastest format for iteration |
| AU scanning | `killall -9 AudioComponentRegistrar` |
| Leak detector | Keep pressing continue to find all leaks |
| Member order | LookAndFeel before components using it |
| Paint performance | Avoid allocation, use member variables |
| Parameters | Callbacks can be on audio thread |
| Singletons | Avoid in plugins - causes multi-instance issues |
| Windows runtime | Statically link for reliability |

---

*Source: [melatonin.dev](https://melatonin.dev/blog/big-list-of-juce-tips-and-tricks/) by sudara*
