# Vulkan Learning Notes

## Big Picture

Vulkan is a low-level graphics API. It gives the application explicit control
over the GPU, memory, rendering operations, and synchronization.

This control can improve performance, but it also means the application must
describe many details that higher-level graphics APIs manage automatically.

## GLFW

GLFW is not part of Vulkan. It provides platform-independent window creation,
input handling, and the connection between a native window and Vulkan.

On macOS, GLFW helps create a Vulkan surface without our code directly dealing
with Cocoa and Metal window-system details.

## Instance

`VkInstance` is the application's connection to the Vulkan library.

It contains application information and enables instance-level extensions and
validation layers. It does not represent a specific GPU.

## Extensions and Validation Layers

Extensions add optional Vulkan functionality. Examples used on macOS include:

- `VK_KHR_surface`: supports presenting images to a window system.
- `VK_EXT_metal_surface`: connects a Vulkan surface to Metal.
- `VK_KHR_portability_enumeration`: exposes portable Vulkan implementations
  such as MoltenVK.

`VK_LAYER_KHRONOS_validation` is a runtime correctness checker. It intercepts
Vulkan calls and reports problems such as:

- Invalid parameters or object usage
- Incorrect image layouts
- Missing synchronization
- Invalid resource lifetimes
- Unsupported features or extensions

It reports mistakes but does not fix them. It is mainly used during development.

## Surface

`VkSurfaceKHR` represents the connection between Vulkan and a native window.

Vulkan renders images; the surface tells Vulkan where those images may be
presented. GLFW hides the platform-specific surface implementation.

## Physical and Logical Devices

`VkPhysicalDevice` is a handle used to inspect and select an available GPU.
It lets us query the GPU's properties, features, extensions, and queue families.

`VkDevice` is the logical device our application creates from the selected
physical device. Most actual Vulkan resources and operations use this device.

Mental model:

```text
VkPhysicalDevice = inspect and choose hardware
VkDevice         = application connection used to operate that hardware
```

## Queues and Queue Families

A queue is a channel through which the application submits work to the GPU.

A queue family describes a group of queues with particular capabilities:

- Graphics queues can execute rendering commands.
- Present queues can send completed images to the window surface.

Graphics and presentation may use the same queue family. On the Apple M3 Pro,
both are currently queue family `0`.

## Swapchain

`VkSwapchainKHR` owns a collection of images that can be presented to the
window. These images are reusable slots, not permanently numbered frames.

A typical frame will:

1. Acquire an available swapchain image.
2. Render into that image.
3. Present the completed image.
4. Return it to the pool for reuse later.

Three swapchain images provide three reusable image slots. This is commonly
associated with triple buffering, although the exact scheduling is controlled
by the presentation system.

## Image Format and Present Mode

The selected format is normally `VK_FORMAT_B8G8R8A8_SRGB`:

- B, G, R, and A describe the component order.
- Each component uses 8 bits.
- `SRGB` provides color conversion appropriate for display output.

The present mode controls how rendered images enter the display queue:

- `FIFO`: queued in order and synchronized to display refresh; always supported.
- `MAILBOX`: newer completed images may replace older queued images; useful for
  low-latency triple buffering.
- `IMMEDIATE`: presents without waiting for refresh and may cause tearing.

## Images and Image Views

`VkImage` owns or represents image data.

`VkImageView` describes how Vulkan should access and interpret that image,
including its format, color/depth aspect, mip levels, and array layers.

It is similar to a typed lens over an image. It does not copy the pixels.

Important image-view fields:

- `aspectMask`: selects color, depth, or stencil data.
- `baseMipLevel`: selects the first mip level.
- `levelCount`: selects how many mip levels are visible.
- `baseArrayLayer` and `layerCount`: select image-array or cube-map layers.

A mip level is a progressively smaller-resolution version of an image. The
swapchain images currently expose only mip level `0`, the full resolution.

## Render Pass

`VkRenderPass` describes how rendering uses attachments. An attachment is an
image used as a rendering destination, such as a color or depth image.

Our render pass describes one color attachment:

- Its format matches the swapchain image format.
- `loadOp = CLEAR` clears it when rendering begins.
- `storeOp = STORE` preserves the rendered pixels for presentation.
- It is used in `COLOR_ATTACHMENT_OPTIMAL` layout while drawing.
- It finishes in `PRESENT_SRC_KHR` layout so it can be displayed.

The render pass is a contract or recipe. It does not identify a particular
swapchain image and does not execute drawing by itself.

## Shaders and Graphics Pipeline

A shader is a small program executed by the GPU. We write shaders in GLSL and
compile them into SPIR-V binaries, which Vulkan loads into shader modules.

Our vertex shader runs once for each of the triangle's three vertices. It uses
`gl_VertexIndex` to select a position and color:

```text
Vertex 0 -> position 0 + red
Vertex 1 -> position 1 + green
Vertex 2 -> position 2 + blue
```

Our fragment shader runs for the generated fragments, which roughly correspond
to candidate pixels covered by the triangle. It writes a final color.

The graphics pipeline connects several fixed stages:

```text
Vertex shader
    |
Input assembly: group every three vertices into a triangle
    |
Rasterizer: determine which screen fragments the triangle covers
    |
Fragment shader: calculate colors
    |
Color blending: write colors to the attachment
```

The pipeline layout describes resources shaders may access. It is empty for
this lesson because our shader positions and colors are written directly into
the shader source.

Creating a pipeline does not draw anything. A future command buffer must bind
the pipeline and issue `vkCmdDraw`.

## How the Pieces Connect

```text
VkInstance
    |
VkPhysicalDevice -> choose GPU
    |
VkDevice -> operate GPU
    |
VkQueue -> submit GPU work

Window
  |
VkSurfaceKHR
  |
VkSwapchainKHR
  |
VkImage -> VkImageView -> VkFramebuffer
                            |
                       VkRenderPass
                            |
                    graphics commands
                            |
                         present
```

`VkFramebuffer` is an upcoming link that pairs actual image views with the
attachments described by a render pass.

## Rough Diagrams

### 1. Vulkan Object Overview

```text
Application
    |
    v
VkInstance ---------------- Validation Layer
    |                         checks our Vulkan calls
    v
VkPhysicalDevice
    "Which GPU should I use?"
    |
    v
VkDevice
    "My usable connection to that GPU"
    |
    +------------------+
    |                  |
    v                  v
Graphics Queue      Present Queue
draw work           display completed images
```

### 2. Window and Swapchain

```text
GLFW Window
    |
    v
VkSurfaceKHR
    "Vulkan connection to this window"
    |
    v
VkSwapchainKHR
    |
    +-- Image 0: available/rendering/displaying
    +-- Image 1: available/rendering/displaying
    +-- Image 2: available/rendering/displaying
```

The image numbers identify reusable slots. They do not mean frame 0, frame 1,
and frame 2 permanently.

### 3. Journey of One Frame

```text
Acquire an available swapchain image
                 |
                 v
        Image layout: UNDEFINED
                 |
                 v
        Begin the render pass
                 |
                 v
 Image layout: COLOR_ATTACHMENT_OPTIMAL
                 |
                 v
       Clear image and draw
                 |
                 v
         End the render pass
                 |
                 v
    Image layout: PRESENT_SRC_KHR
                 |
                 v
        Present it to the window
                 |
                 v
       Image eventually becomes
            available again
```

### 4. Image, View, Framebuffer, and Render Pass

```text
VkImage
actual pixel storage
    |
    v
VkImageView
"Use this image as BGRA color, mip 0, layer 0"
    |
    v
VkFramebuffer
"This particular image view fills attachment slot 0"
    |
    v
VkRenderPass
"Clear attachment 0, draw into it, then prepare it for presentation"
```

Another mental model:

```text
VkImage       = the paper
VkImageView   = how we look at the paper
VkFramebuffer = the paper placed on today's drawing desk
VkRenderPass  = the instructions for using the drawing desk
```

### 5. Three Reusable Swapchain Slots

```text
Time       Image 0          Image 1          Image 2
----       -------          -------          -------
T1         Rendering        Available        Available
T2         Displaying       Rendering        Available
T3         Available        Displaying       Rendering
T4         Rendering        Available        Displaying
```

This is only a simplified timeline. Real scheduling depends on the present
mode, synchronization, GPU speed, and display refresh.

### 6. What We Have and What Is Missing

```text
[DONE] Window and surface
          |
[DONE] Select physical device
          |
[DONE] Create logical device and queues
          |
[DONE] Create swapchain images
          |
[DONE] Create image views
          |
[DONE] Describe render pass
          |
[DONE] Create graphics pipeline and shaders
          |
[NEXT] Create framebuffers
          |
[TODO] Record command buffers
          |
[TODO] Synchronize, submit, and present
          |
       Visible triangle
```

## Current Progress

Lessons completed:

1. Instance and physical-device discovery
2. Window surface
3. Queue-family discovery
4. Logical device and queues
5. Swapchain support
6. Swapchain and swapchain images
7. Image views
8. Render pass
9. Graphics pipeline and shaders

The window is still blank because we have not created framebuffers, command
buffers, synchronization objects, or the draw loop yet.
