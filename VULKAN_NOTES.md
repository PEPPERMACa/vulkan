# Vulkan Learning Notes

## Big Picture

Vulkan is a low-level graphics API. It gives the application explicit control
over the GPU, memory, rendering operations, and synchronization.

This control can improve performance, but it also means the application must
describe many details that higher-level graphics APIs manage automatically.

## Current Study Progress

The code is currently at Lesson 12.

Lessons completed:

1. Instance and physical-device discovery
2. Window surface
3. Queue-family discovery
4. Logical device and queues
5. Swapchain support checks
6. Swapchain and swapchain images
7. Image views
8. Render pass
9. Shaders and graphics pipeline
10. Framebuffers
11. Command pool and command buffers
12. Synchronization, submit, and present

Current state:

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
[DONE] Compile shaders and create shader modules
          |
[DONE] Create graphics pipeline
          |
[DONE] Create framebuffers
          |
[DONE] Record command buffers
          |
[DONE] Synchronize, submit, and present
          |
[DONE] Visible triangle
```

The app now acquires a swapchain image, submits the matching command buffer to
the graphics queue, and presents the rendered image. This is the first point
where the triangle should appear on screen.

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

Important render-pass pieces in the current code:

- Attachment description: what kind of image will be used.
- Attachment reference: which attachment slot the subpass reads/writes.
- Subpass: one step of rendering work inside the render pass.
- Subpass dependency: ordering and memory rules around the subpass.

The current render pass has one subpass and one color attachment. Later, each
framebuffer will provide the actual swapchain image view for attachment slot
`0`.

## Shaders, Shader Modules, and SPIR-V

A shader is a small program executed by the GPU. We write shaders in GLSL and
compile them into SPIR-V binaries, which Vulkan loads into shader modules.

In this project:

```text
shaders/triangle.vert -> glslc -> triangle.vert.spv
shaders/triangle.frag -> glslc -> triangle.frag.spv
```

`glslc` is the shader compiler. GLSL is the human-readable shader language.
SPIR-V is the compiled binary format Vulkan accepts.

A `VkShaderModule` is a Vulkan object created from SPIR-V bytes. It means:

```text
"Here is one compiled shader program that can be used in a pipeline stage."
```

A shader module does not draw anything by itself. The graphics pipeline says
which stage uses the module, and which entry function to run.

Our vertex shader runs once for each of the triangle's three vertices. It uses
`gl_VertexIndex` to select a position and color:

```text
Vertex 0 -> position 0 + red
Vertex 1 -> position 1 + green
Vertex 2 -> position 2 + blue
```

Our fragment shader runs for the generated fragments, which roughly correspond
to candidate pixels covered by the triangle. It writes a final color.

The vertex shader outputs RGB color as a `vec3`. The fragment shader receives
that color, adds alpha, and writes a `vec4`:

```text
vec3(1.0, 0.0, 0.0) -> red
vec3(0.0, 1.0, 0.0) -> green
vec3(0.0, 0.0, 1.0) -> blue
vec4(color, 1.0)    -> RGB plus fully opaque alpha
```

## Graphics Pipeline

The graphics pipeline is Vulkan's drawing recipe. It gathers shader stages and
fixed-function settings into one object.

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

The current graphics pipeline includes:

- Shader stages: one vertex shader and one fragment shader.
- Vertex input: empty, because the triangle data is hardcoded in the vertex
  shader instead of coming from a vertex buffer.
- Input assembly: groups every three vertices into a triangle.
- Viewport: maps rendering into the swapchain extent.
- Scissor: limits rendering to the swapchain rectangle.
- Rasterizer: fills triangles and turns them into fragments.
- Multisampling: currently one sample per pixel, so no MSAA yet.
- Color blending: writes RGBA output directly, with blending disabled.
- Pipeline layout: currently empty because there are no uniforms, textures,
  descriptor sets, or push constants.
- Render pass compatibility: the pipeline is built for subpass `0` of the
  current render pass.

The pipeline layout describes resources shaders may access. It is empty for
this lesson because our shader positions and colors are written directly into
the shader source.

Creating a pipeline does not draw anything. A future command buffer must bind
the pipeline and issue `vkCmdDraw`.

## Command Pool and Command Buffers

Vulkan work is submitted through command buffers. A command buffer is a recorded
list of GPU commands.

The command pool owns memory used by command buffers. It is tied to a queue
family, so our command pool uses the graphics queue family.

For the current app, we record one command buffer per framebuffer:

```text
commandBuffer[0] -> framebuffer[0] -> imageView[0]
commandBuffer[1] -> framebuffer[1] -> imageView[1]
commandBuffer[2] -> framebuffer[2] -> imageView[2]
```

Each command buffer records the same drawing recipe, but targets a different
framebuffer:

```text
Begin command buffer
    Begin render pass for framebuffer[i]
        Bind graphics pipeline
        Draw 3 vertices
    End render pass
End command buffer
```

The `vkCmdDraw(commandBuffer, 3, 1, 0, 0)` call means:

- Draw 3 vertices.
- Draw 1 instance.
- Start at vertex index 0.
- Start at instance index 0.

The vertex shader uses `gl_VertexIndex` values `0`, `1`, and `2` to choose the
three hardcoded triangle positions and colors.

## Synchronization, Submit, and Present

Vulkan does not automatically keep the CPU, graphics queue, swapchain, and
presentation engine in lockstep. We use synchronization objects to describe the
safe order of work.

Current synchronization objects:

- `imageAvailableSemaphore`: rendering waits until the swapchain image is ready.
- `renderFinishedSemaphore`: presentation waits until rendering is finished.
- `inFlightFence`: the CPU waits until the previous submitted frame is done.

One frame now follows this order:

```text
Wait for previous frame fence
        |
Acquire swapchain image
        |
Reset fence
        |
Submit commandBuffer[imageIndex]
    waits on imageAvailableSemaphore
    signals renderFinishedSemaphore
    signals inFlightFence when complete
        |
Present image
    waits on renderFinishedSemaphore
```

The fence is created in the signaled state so the first frame can start without
waiting forever. After that, each submission signals the fence when the GPU has
finished the submitted work.

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

`VkFramebuffer` pairs actual image views with the attachments described by a
render pass.

The render pass says what attachment slots exist. The framebuffer says which
actual image views fill those slots for a specific render target.

Because the swapchain owns multiple images, we create one framebuffer per
swapchain image view:

```text
Framebuffer 0 -> image view 0 -> swapchain image 0
Framebuffer 1 -> image view 1 -> swapchain image 1
Framebuffer 2 -> image view 2 -> swapchain image 2
```

Each framebuffer uses the same render pass and swapchain extent. Later, when we
acquire a swapchain image, we will choose the framebuffer with the same index
and begin the render pass using that framebuffer.

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

### 6. Lesson 09 Pipeline Setup

```text
GLSL shader source
        |
        v
glslc compiles to SPIR-V
        |
        v
VkShaderModule
        |
        v
VkPipelineShaderStageCreateInfo
        |
        v
VkGraphicsPipelineCreateInfo
        |
        v
VkPipeline
```

Lesson 09 creates the pipeline, but the app still does not issue draw commands.
A future command buffer must begin the render pass, bind the pipeline, and call
`vkCmdDraw`.

### 7. Lesson 10 Framebuffer Setup

```text
VkRenderPass
    says attachment slot 0 is a color target
        |
        v
VkImageView
    describes one swapchain image as a color image
        |
        v
VkFramebuffer
    connects that image view to attachment slot 0
```

Lesson 10 creates render targets for the future command buffers. It still does
not draw because no command buffer has begun a render pass, bound the pipeline,
or issued `vkCmdDraw` yet.

### 8. Lesson 11 Command Buffer Setup

```text
VkCommandPool
    owns command-buffer memory for the graphics queue family
        |
        v
VkCommandBuffer
    records commands for one framebuffer
        |
        v
Begin render pass
Bind graphics pipeline
vkCmdDraw 3 vertices
End render pass
```

Lesson 11 records drawing commands, but the app still does not show the
triangle because nothing submits those command buffers to the graphics queue
or presents the rendered swapchain image yet.

### 9. Lesson 12 Submit and Present

```text
vkAcquireNextImageKHR
    gives imageIndex
        |
        v
vkQueueSubmit
    submits commandBuffer[imageIndex] to graphics queue
        |
        v
vkQueuePresentKHR
    presents swapchain image imageIndex
```

Lesson 12 is the first lesson where the recorded command buffer is executed and
the rendered swapchain image is presented to the window.
