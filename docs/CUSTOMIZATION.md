# Customization

### Config file

- `DXMT_CONFIG_FILE=/xxx/dxmt.conf` Sets path to the configuration file. Check `dxmt.conf` in the project directory for reference.
- `DXMT_CONFIG="d3d11.preferredMaxFrameRate=30;"` Can be used to set config variables through the environment instead of a configuration file using the same syntax. `;` is used as a seperator.

### MetalFX Spatial Upscaling for Swapchain

Set environment variable `DXMT_METALFX_SPATIAL_SWAPCHAIN=1` to enable MetalFX spatial upscaler on output swapchain. By default it will double the output resolution. Set `d3d11.metalSpatialUpscaleFactor` to a value between 1.0 and 2.0 to change the scale factor.

### Metal Frame Pacing

`d3d11.preferredMaxFrameRate` can be set to enforce the application's frame pacing being controled by Metal. The value must be a factor of your display's refresh rate. (e.g. 15/30/40/60/120 is valid for a 120hz display).