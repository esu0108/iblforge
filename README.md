[English](README.md) | [日本語](README.ja.md)

# iblforge

**A real-time Image-Based Lighting (IBL) baking tool with CPU and GPU acceleration.**

iblforge converts equirectangular HDR environment maps into PBR-ready lighting textures. It provides a configurable baking pipeline with both multithreaded CPU processing and Direct3D 12 GPU acceleration, allowing you to generate, inspect, and export IBL textures in a single application.

## Features

- **Specular IBL** — Generate GGX-prefiltered environment cubemaps with a complete mip chain.
- **Diffuse IBL** — Generate irradiance cubemaps using cosine-weighted hemisphere sampling.
- **BRDF Integration** — Generate split-sum BRDF integration LUTs for physically based rendering.
- **CPU & GPU Acceleration** — Choose between multithreaded CPU processing and Direct3D 12 compute shaders.
- **Configurable Baking** — Adjust texture resolutions, sampling counts, and firefly clamping.
- **Multiple Output Formats** — Export HDR cubemaps in RGBA16F, RGBA32F, R11G11B10F, or BC6H.
- **Command-Line Interface** — Bake and export textures without launching the GUI.
- **3D Preview** — Inspect generated lighting textures on 3D models with adjustable materials and lighting parameters.

## Generated Textures

From a single equirectangular HDR environment map, iblforge generates three DDS textures:

| Output | Description | Default Format |
|---|---|---|
| `*_prefiltered.dds` | GGX-prefiltered specular environment cubemap with a complete mip chain | RGBA16F |
| `*_irradiance.dds` | Diffuse irradiance cubemap generated using cosine-weighted convolution | RGBA16F |
| `*_brdf_lut.dds` | Split-sum BRDF integration lookup texture | RG16F |

### Prefiltered Specular Cubemap

Generated using GGX importance sampling. Each mip level represents a different surface roughness, allowing PBR shaders to efficiently approximate specular image-based lighting.

The roughness-to-mip mapping is:

```cpp
float mipLevel = roughness * (mipCount - 1);
```

### Diffuse Irradiance Cubemap

Generated using cosine-weighted hemisphere sampling. The output stores irradiance normalized by π, so diffuse lighting can be evaluated as:

```hlsl
float3 diffuse = albedo * irradiance;
```

### BRDF Integration LUT

Generated using the split-sum approximation for image-based specular lighting. Its two channels store the scale and bias terms used to evaluate the specular BRDF.

```hlsl
float2 brdf = brdfLut.Sample(samplerState, float2(NdotV, roughness));

float3 specular = prefilteredColor * (F0 * brdf.x + brdf.y);
```

## Screenshot

![iblforge screenshot](docs/screenshot.png)

The application provides a dockable interface with three main panels:

- **IBL Bake** — HDR environment preview and baking configuration.
- **3D View** — Real-time PBR preview of the selected model.
- **Material / Lighting** — Material properties, lighting controls, and BRDF LUT visualization.

## Getting Started

### Requirements

- Windows 10 or later
- Visual Studio 2022 or later
- CMake 3.21 or later
- A Direct3D 12-compatible GPU (required for GPU baking and 3D preview)

### Building

Clone the repository:

```bash
git clone https://github.com/esu0108/iblforge.git
cd iblforge
```

Configure the project:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

Build:

```bash
cmake --build build --config Release
```

CMake automatically downloads the required dependencies through FetchContent. Sample 3D models are also downloaded and converted during configuration and build.

The executable is generated at:

```text
build/Release/iblforge.exe
```

### GUI Usage

Launch the application:

```bash
iblforge.exe
```

1. Load an HDR environment map using `File > Open HDRI...` or drag and drop an `.hdr` file onto the window.
2. Configure the baking parameters in the **IBL Bake** panel.
3. Select CPU or GPU processing.
4. Click **Bake** to generate the lighting textures.
5. Inspect the results in the 3D viewer.
6. Export the generated textures using `File > Save baked textures...`.

Baked textures remain in memory until explicitly saved.

Previously generated textures can be loaded using `File > Import baked textures...`. Select any one of the three DDS files, and the other two are located automatically.

### Command-Line Usage

iblforge supports headless baking, allowing textures to be generated and exported directly from the command line without launching the GUI.

```bash
iblforge.exe --bake-only input.hdr output_directory
```

Example:

```bash
iblforge.exe --bake-only environment.hdr output --size 256 --samples 2048
```

#### Command-Line Options

| Option | Description | Default |
|---|---|---|
| `--bake-only` | Run without opening the GUI and save the results directly | Disabled |
| `--cpu` | Use the CPU baking implementation | GPU |
| `--gpu` | Use Direct3D 12 compute shaders | GPU |
| `--src N` | Source cubemap resolution | 512 |
| `--size N` | Prefiltered cubemap resolution | 256 |
| `--samples N` | Specular prefilter sampling count | 1024 |
| `--lut N` | BRDF LUT resolution | 256 |
| `--lut-samples N` | BRDF LUT sampling count | 1024 |
| `--irr N` | Irradiance cubemap resolution | 32 |
| `--irr-samples N` | Irradiance sampling count | 2048 |
| `--clamp N` | Maximum sample value; 0 disables clamping | 0 |
| `--format FORMAT` | Cubemap output format | `f16` |
| `--out PATH` | Output directory | — |

Supported output formats:

- `f16` — RGBA16F
- `f32` — RGBA32F
- `r11g11b10` — R11G11B10F
- `bc6h` — BC6H

The output directory can also be specified as the second positional argument.

## Baking

### CPU and GPU Implementations

iblforge provides two implementations of the IBL baking pipeline.

**CPU**

- Multithreaded processing across available CPU cores.
- Suitable for systems without GPU compute support or when GPU stability is a concern.

**GPU**

- Powered by Direct3D 12 compute shaders.
- Performs equirectangular-to-cubemap conversion, mip generation, specular prefiltering, irradiance convolution, and BRDF LUT generation on the GPU.
- Uses the same sampling tables as the CPU implementation to maintain consistent results.
- Divides heavy workloads into smaller dispatch batches to reduce the risk of GPU timeouts.
- Supports asynchronous execution to keep the application responsive.

Both implementations support the same output formats and file naming conventions. Minor floating-point differences between CPU and GPU results are expected.

### Output Formats

The prefiltered and irradiance cubemaps support four output formats:

| Format | DXGI Format | Relative Size vs. RGBA32F | Description |
|---|---|---:|---|
| RGBA16F | R16G16B16A16_FLOAT | 1/2 | Recommended default |
| RGBA32F | R32G32B32A32_FLOAT | 1 | Full precision |
| R11G11B10F | R11G11B10_FLOAT | 1/4 | Compact HDR format without alpha |
| BC6H | BC6H_UF16 | 1/16 | Compressed HDR format |

The BRDF LUT is always exported as RG16F.

BC6H compression is performed on the CPU using parallel processing.

### Canceling a Bake

An ongoing baking operation can be canceled at any time using the **Cancel** button.

Both CPU and GPU implementations check for cancellation between processing batches. When canceled, partial results are discarded and the previously completed bake is preserved.

## 3D Preview

iblforge includes a Direct3D 12-based PBR viewer for inspecting generated IBL textures.

### Sample Models

The viewer includes the following sample models:

| Model | Description |
|---|---|
| Stanford Bunny | Default model |
| Armadillo | Detailed organic geometry |
| Happy Buddha | Sculpted geometry |
| Lucy | High-detail scanned model |
| Sphere | Built-in fallback model |

The sample models are sourced from [alecjacobson/common-3d-test-models](https://github.com/alecjacobson/common-3d-test-models) at a pinned commit.

### Controls

| Input | Action |
|---|---|
| Left mouse drag | Orbit camera |
| Mouse wheel | Zoom |
| `TAB` | Toggle UI visibility |
| `CTRL + O` | Open HDR environment |
| `CTRL + S` | Save baked textures |
| `CTRL + I` | Import baked textures |

The interface uses Dear ImGui Docking, allowing panels to be rearranged, split, and organized into tabs.

## Technical Details

### Coordinate System

iblforge uses a left-handed, Y-up coordinate system compatible with Direct3D.

The equirectangular mapping is defined as:

```cpp
u = 0.5f + atan2(x, z) / (2.0f * PI);
v = acos(y) / PI;
```

The center of the environment map corresponds to the positive Z direction.

Cubemap face ordering follows the Direct3D `TextureCube` convention:

```text
+X, -X, +Y, -Y, +Z, -Z
```

## Dependencies

iblforge uses the following third-party libraries:

| Library | Purpose | Version |
|---|---|---|
| [DirectXTex](https://github.com/microsoft/DirectXTex) | HDR image loading and DDS texture I/O | oct2025 |
| [Dear ImGui](https://github.com/ocornut/imgui) | GUI and docking interface | v1.93.0-docking |
| [rapidobj](https://github.com/guybrush77/rapidobj) | OBJ loading for the build-time mesh converter | v1.1 |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | Mesh optimization for the build-time mesh converter | v1.3 |

All dependencies are automatically downloaded through CMake FetchContent.