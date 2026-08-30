[日本語](README.md) | **English**

# Beatrice Forge

<img width="962" height="721" alt="Beatrice Forge screenshot" src="https://github.com/user-attachments/assets/b8533872-587d-47a8-b721-ce1927f2f685" />

Beatrice Forge is a modified Windows version of the Project Beatrice voice-conversion VST3 plug-in.

This repository preserves the core voice-conversion processing and interface structure of the official version while adding preset management, input audio cleanup, level meters, external output, recording, help text, and other features.

The model files and the non-public inference library are not included in this repository.

### Version history

- **0.9.0**: Release

The current Forge release is based on Project Beatrice `2.0.0-rc.3`. The upstream version and the Forge product version are managed separately.

For the official project specifications, models, and licensing information, refer to the following primary sources:

- [Project Beatrice official website](https://prj-beatrice.com)
- [Project Beatrice / beatrice-vst](https://github.com/prj-beatrice/beatrice-vst)

## Basic operation

1. Select a conversion model under MODEL.
2. Select the target speaker under VOICE.
3. Adjust GAIN and VOICE SHAPE.
4. Configure external output or recording on the IN/OUT tab if needed.

Hover over a control to display a tooltip explaining how to use it. Japanese help text is shown in Japanese environments; the official English terminology is used in other environments.

## Major added features

### Audio cleanup and effects

- **Noise Reduction Boost** — Suppresses low-level noise that may occur with some models when needed.
- **INPUT CLEANUP** — Low Cut reduces unwanted low-frequency content, Light Denoise gently reduces steady background noise, and De-click suppresses brief clicks and lip noise.
- **EFFECTS** — De-Mud reduces muddiness, Presence adds clarity, and Reverb controls the amount, length, and tone of reverberation.
- **Input and converted-output level meters** — Show whether input and converted audio are present and indicate their approximate levels.
- **Legacy-style morph interface** — Retains the familiar morph controls while displaying speaker names and weight values.

### VST-specific external output

The OUTPUT DEVICE control on the IN/OUT tab can send processed audio to WASAPI separately from the host output. This is an additional output path and does not replace the DAW or host routing. Availability depends on the host and audio environment. ASIO output cannot be selected directly through this VST feature.

### Recording

The RECORDING section on the IN/OUT tab provides the following modes:

| Mode | Recorded content |
| --- | --- |
| OFF | No recording |
| Output | Converted output |
| Input/Output Separate | Input and output in separate files |
| Input/Output L-R | Input on the left channel and output on the right channel |

The default destination is the current user's Windows Music folder. Use BROWSE to change the destination or base filename. Files are created using the following names, depending on the selected mode:

~~~~text
Beatrice-Forge-Rec-YYYYMMDD-XXXXXX-Output.wav
Beatrice-Forge-Rec-YYYYMMDD-XXXXXX-Input.wav
Beatrice-Forge-Rec-YYYYMMDD-XXXXXX-LR.wav
~~~~

`XXXXXX` is assigned by checking existing files for the same date. The numbering starts again on a new date, while destination filenames are checked to prevent existing recordings from being overwritten. To stop a recording, use STOP without changing the recording menu while recording is active.

The recording mode and destination are saved in the plug-in state passed to the VST host. Whether recording is active and any running WASAPI stream are not saved or automatically resumed.

### Preset lists

You can register multiple snapshots of the current state—including the loaded model, speaker, and other settings—as presets and recall any of them at any time by clicking it. Changes to settings or the speaker after registration are reflected in the preset in real time. Preset names can be changed freely, although changing the speaker automatically renames the preset.
Preset lists can be organized across multiple tabs.
You can rename lists and entries, reorder lists, create and save blank presets, and import or export the currently selected list.

Preset and plug-in settings are passed to the host as VST3 state data. Their storage location and restoration timing therefore depend on the DAW or VST host. Use EXPORT LIST and IMPORT LIST when transferring preset lists between environments.

## Build

Building this repository requires Visual Studio 2022, CMake, the Git submodules, and `beatrice.lib`, which must be individually authorized by Project Beatrice. `beatrice.lib` is not included in public files and is not downloaded automatically.

The minimum build procedure is:

~~~~powershell
git submodule update --init --recursive
cmake -S . -B build/vs3 -G "Visual Studio 17 2022" -A x64
cmake --build build/vs3 --config Release
~~~~

The VST3 package is generated under `build/vs3/VST3/Release/`. It is not automatically copied to a DAW VST folder.

- The primary target is a 64-bit plug-in host.
- The inference library `beatrice.lib` is not downloaded automatically. Model files and supporting data are not included in this repository.
- Distribution packages contain only the plug-in; official models and supporting data are not bundled or downloaded automatically.
- The VST-specific external output supports WASAPI only; ASIO is not available through this feature.
- Enabling external output does not change the host input/output routing.
- External output or recording may fail to start depending on device availability, sample rate, exclusive access, or host thread behavior.

## License

The Beatrice Forge VST3 plug-in is provided under the MIT License. See [LICENSE.txt](LICENSE.txt) for the copyright notice and license terms.
