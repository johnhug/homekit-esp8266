# WS281x

HomeKit interface for ws281x led strip.  Presents a Control service as well as 7 identical Color services (0-6). Different animations make use of the 1-7 colors in their animation and controlled by the custom characteristics on the Control service.  For controlling or making a scene I use the [Controller for Homekit](https://apps.apple.com/us/app/controller-for-homekit/id1198176727) app. Essentially I wanted to control the entirety of the device via Homekit and not write a separate UI.

Uses an underlying WS2812 specific DMA library however added the BGR to RGB ordering flip in the usage to cover WS2811.

Currently used with a 300 led WS2812 strip and a 100 led WS2811 rope.

## Animations

_Speed_ affects all (minus Solid) consistently, it changes the delay between repaints.

_Reverse_ affects any directional animation, it changes the paint from 0 -> n to n -> 0.

"Enabled Colors" is the set of Color-n services that have their On characteristic true.

### Solid

Simple solid color.  Uses Color-0 and paints the entire strip that color.

_Fade_ and _Density_ have no affect.

### Chase

Static colors of the enabled colors repeated down the strip with a chase of each pixel in the repeat going to full brightness and diminishing.

_Fade_ affects the rate of diminishing.

_Density_ has no affect.


### Twinkle

Static pixels of the enabled colors repeated down the strip with randon pixels fading into full brightness and diminishing.

_Fade_ affects the rate of increasing/diminishing.

_Density_ affects the number of random pixels triggered.

### Sequence

Sequence of enabled colors one pixel each moving down the strip.

_Fade_ and _Density_ have no affect.

### Stripes

Sequence of enabled colors with the pixels per color variable moving down the strip,

_Density_ is treated as a percent of total leds per stripe.

_Fade_ has no affect.

### Comets

Comets!!!  The nucleus of the comet is a white pixel with a tail of fading color.  The enabled colors are made into a sequence of comets that move down the strip.

_Density_ is treated as a percent of total leds per comet.

_Fade_ is the rate at which the tail diminishes to black.

### Fireworks

Random pixels light up with a randomly chosen enabled color to full brightness and fade away.

_Fade_ affects the rate of diminishing.

_Density_ affects the number of random pixels triggered.

### Cycle

Entire strip is a single color.  It cycles through the enabled colors while mixing from each to the next.

_Density_ changes the duration of each color in repaints.

_Fade_ changes the percentage within that duration when the mixing to the next color starts.

