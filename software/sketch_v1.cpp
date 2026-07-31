/*
  615 Labs Erlenmeyer Flask SAO
  CH32V003F4U6 QFN-20

  Animation layout:

  Bubble path:
    LED4 -> LED3 -> LED2 -> LED1
    Bottom                    Top

  Flask liquid:
    LED5, LED6, LED7, LED8

  Confirmed GPIO assignments:

    LED1 -> PD7
    LED2 -> PD4
    LED3 -> PA1
    LED4 -> PD3
    LED5 -> PC4
    LED6 -> PD2
    LED7 -> PC3
    LED8 -> PC0
*/

#include <Arduino.h>
#include <ch32v00x.h>

/* =========================================================
   BUBBLE TUNING
   ========================================================= */

const uint16_t BUBBLE_GAP_MIN_MS = 85;
const uint16_t BUBBLE_GAP_MAX_MS = 180;

const uint8_t BUBBLE_SPEED_MIN = 3;
const uint8_t BUBBLE_SPEED_MAX = 7;

const uint8_t BUBBLE_BRIGHTNESS_MIN = 35;
const uint8_t BUBBLE_BRIGHTNESS_MAX = 65;

const uint8_t DOUBLE_BUBBLE_CHANCE_PERCENT = 2;

const uint16_t SECOND_BUBBLE_DELAY_MIN_MS = 20;
const uint16_t SECOND_BUBBLE_DELAY_MAX_MS = 80;

/* =========================================================
   LIQUID TUNING
   ========================================================= */

/*
  Lower base values reduce the average duty cycle.
  LED7 is slightly brighter to keep the center of the flask
  visually fuller.
*/

const uint8_t LIQUID_LED5_BASE = 55;
const uint8_t LIQUID_LED6_BASE = 68;
const uint8_t LIQUID_LED7_BASE = 82;
const uint8_t LIQUID_LED8_BASE = 66;

/*
  Individual local shimmer amount.
  Lower values produce smoother liquid.
  Higher values produce more visible flicker.
*/

const uint8_t LIQUID_SHIMMER_AMOUNT = 58;

/*
  The entire liquid slowly cycles between these brightness
  multipliers.

  255 = 100%
  192 = 75%
  128 = 50%
*/

const uint8_t LIQUID_CYCLE_MIN = 115;
const uint8_t LIQUID_CYCLE_MAX = 150;

/*
  Controls how quickly the overall liquid brightness cycles.

  Larger values make the breathing cycle slower.
*/

const uint8_t LIQUID_CYCLE_DIVIDER = 8;

/*
  Temporary brightness disturbance when a new bubble forms.
*/

const uint8_t LIQUID_BUBBLE_SURGE = 42;

/*
  Controls how quickly the bubble surge fades.
*/

const uint8_t LIQUID_SURGE_DECAY = 2;

/* =========================================================
   SOFTWARE PWM SETTINGS
   ========================================================= */

const uint8_t PWM_STEPS = 32;
const uint16_t PWM_SLICE_US = 180;

/*
  Approximate animation frame duration.

  This is used for bubble scheduling rather than PWM timing.
*/

const uint8_t APPROX_FRAME_MS = 7;

/* =========================================================
   LED DEFINITIONS
   ========================================================= */

struct LedChannel
{
  GPIO_TypeDef *port;
  uint16_t pin;
};

const LedChannel LEDS[] =
{
  {GPIOD, GPIO_Pin_7},  // LED1
  {GPIOD, GPIO_Pin_4},  // LED2
  {GPIOA, GPIO_Pin_1},  // LED3
  {GPIOD, GPIO_Pin_3},  // LED4
  {GPIOC, GPIO_Pin_4},  // LED5
  {GPIOD, GPIO_Pin_2},  // LED6
  {GPIOC, GPIO_Pin_3},  // LED7
  {GPIOC, GPIO_Pin_0}   // LED8
};

const uint8_t LED_COUNT =
  sizeof(LEDS) / sizeof(LEDS[0]);

enum LedNumber
{
  LED1_INDEX = 0,
  LED2_INDEX = 1,
  LED3_INDEX = 2,
  LED4_INDEX = 3,
  LED5_INDEX = 4,
  LED6_INDEX = 5,
  LED7_INDEX = 6,
  LED8_INDEX = 7
};

/*
  Physical bubble travel order.
*/

const uint8_t BUBBLE_PATH[] =
{
  LED4_INDEX,
  LED3_INDEX,
  LED2_INDEX,
  LED1_INDEX
};

const uint8_t BUBBLE_PATH_LENGTH =
  sizeof(BUBBLE_PATH) / sizeof(BUBBLE_PATH[0]);

/* =========================================================
   ANIMATION STATE
   ========================================================= */

struct Bubble
{
  bool active;

  /*
    Fixed-point position:

      0   = LED4
      256 = LED3
      512 = LED2
      768 = LED1
  */

  uint16_t position;
  uint8_t speed;
  uint8_t brightness;
};

Bubble bubbles[2];

uint8_t ledBrightness[LED_COUNT];

uint32_t frameCounter = 0;

uint16_t framesUntilNextBubble = 1;
uint16_t framesUntilSecondBubble = 0;

bool secondBubblePending = false;

uint8_t liquidSurge = 0;

uint16_t randomState = 0xA3C5;

/* =========================================================
   RANDOM NUMBER FUNCTIONS
   ========================================================= */

uint16_t nextRandom()
{
  uint16_t value = randomState;

  value ^= value << 7;
  value ^= value >> 9;
  value ^= value << 8;

  randomState = value;

  return value;
}

uint16_t randomRange(
  uint16_t minimum,
  uint16_t maximum
)
{
  if (maximum <= minimum)
  {
    return minimum;
  }

  return minimum +
         (nextRandom() % (maximum - minimum + 1));
}

bool randomChance(uint8_t percent)
{
  return (nextRandom() % 100) < percent;
}

/* =========================================================
   GENERAL UTILITY FUNCTIONS
   ========================================================= */

uint16_t millisecondsToFrames(uint16_t milliseconds)
{
  uint16_t frames = milliseconds / APPROX_FRAME_MS;

  if (frames == 0)
  {
    frames = 1;
  }

  return frames;
}

uint8_t clampBrightness(int16_t value)
{
  if (value < 0)
  {
    return 0;
  }

  if (value > 255)
  {
    return 255;
  }

  return (uint8_t)value;
}

void clearBrightness()
{
  for (uint8_t i = 0; i < LED_COUNT; i++)
  {
    ledBrightness[i] = 0;
  }
}

void addBrightness(
  uint8_t ledIndex,
  uint16_t amount
)
{
  if (ledIndex >= LED_COUNT)
  {
    return;
  }

  uint16_t total =
    (uint16_t)ledBrightness[ledIndex] + amount;

  if (total > 255)
  {
    total = 255;
  }

  ledBrightness[ledIndex] = (uint8_t)total;
}

/*
  Produces a repeating triangle wave from 0 through 255.
*/

uint8_t triangleWave(uint16_t phase)
{
  uint8_t position = phase & 0xFF;

  if (position < 128)
  {
    return position * 2;
  }

  return (255 - position) * 2;
}

/*
  Produces a centered wave from approximately -128 to +127.
*/

int16_t centeredTriangleWave(uint16_t phase)
{
  return (int16_t)triangleWave(phase) - 128;
}

/*
  Scales an 8-bit brightness value by another 8-bit value.

  255 means full scale.
  128 means approximately half scale.
*/

uint8_t scaleBrightness(
  uint8_t brightness,
  uint8_t scale
)
{
  return (uint8_t)(
    ((uint16_t)brightness * scale) / 255
  );
}

/* =========================================================
   GPIO CONFIGURATION
   ========================================================= */

void setLedHardware(
  uint8_t ledIndex,
  bool state
)
{
  if (ledIndex >= LED_COUNT)
  {
    return;
  }

  if (state)
  {
    GPIO_SetBits(
      LEDS[ledIndex].port,
      LEDS[ledIndex].pin
    );
  }
  else
  {
    GPIO_ResetBits(
      LEDS[ledIndex].port,
      LEDS[ledIndex].pin
    );
  }
}

void turnAllLedsOff()
{
  for (uint8_t i = 0; i < LED_COUNT; i++)
  {
    setLedHardware(i, false);
  }
}

void configureOutput(
  GPIO_TypeDef *port,
  uint16_t pin
)
{
  GPIO_InitTypeDef gpioConfig = {0};

  gpioConfig.GPIO_Pin = pin;
  gpioConfig.GPIO_Mode = GPIO_Mode_Out_PP;
  gpioConfig.GPIO_Speed = GPIO_Speed_10MHz;

  GPIO_Init(port, &gpioConfig);
}

void disablePA1PA2OscillatorRemap()
{
#if defined(GPIO_Remap_PA1_2)

  GPIO_PinRemapConfig(
    GPIO_Remap_PA1_2,
    DISABLE
  );

#elif defined(GPIO_Remap_PA1PA2)

  GPIO_PinRemapConfig(
    GPIO_Remap_PA1PA2,
    DISABLE
  );

#else

  AFIO->PCFR1 &= ~((uint32_t)0x00008000);

#endif
}

/* =========================================================
   BUBBLE CONTROL
   ========================================================= */

int8_t findAvailableBubble()
{
  for (uint8_t i = 0; i < 2; i++)
  {
    if (!bubbles[i].active)
    {
      return (int8_t)i;
    }
  }

  return -1;
}

bool spawnBubble()
{
  int8_t bubbleIndex = findAvailableBubble();

  if (bubbleIndex < 0)
  {
    return false;
  }

  bubbles[bubbleIndex].active = true;
  bubbles[bubbleIndex].position = 0;

  bubbles[bubbleIndex].speed = randomRange(
    BUBBLE_SPEED_MIN,
    BUBBLE_SPEED_MAX
  );

  bubbles[bubbleIndex].brightness = randomRange(
    BUBBLE_BRIGHTNESS_MIN,
    BUBBLE_BRIGHTNESS_MAX
  );

  /*
    Disturb the liquid when a new bubble forms.
  */

  liquidSurge = LIQUID_BUBBLE_SURGE;

  return true;
}

void scheduleNextMainBubble()
{
  uint16_t delayMs = randomRange(
    BUBBLE_GAP_MIN_MS,
    BUBBLE_GAP_MAX_MS
  );

  framesUntilNextBubble =
    millisecondsToFrames(delayMs);
}

void schedulePossibleSecondBubble()
{
  if (!randomChance(
        DOUBLE_BUBBLE_CHANCE_PERCENT
      ))
  {
    secondBubblePending = false;
    return;
  }

  uint16_t delayMs = randomRange(
    SECOND_BUBBLE_DELAY_MIN_MS,
    SECOND_BUBBLE_DELAY_MAX_MS
  );

  framesUntilSecondBubble =
    millisecondsToFrames(delayMs);

  secondBubblePending = true;
}

void updateBubbleScheduler()
{
  if (framesUntilNextBubble > 0)
  {
    framesUntilNextBubble--;
  }
  else
  {
    if (spawnBubble())
    {
      schedulePossibleSecondBubble();
    }

    scheduleNextMainBubble();
  }

  if (secondBubblePending)
  {
    if (framesUntilSecondBubble > 0)
    {
      framesUntilSecondBubble--;
    }
    else
    {
      if (spawnBubble())
      {
        secondBubblePending = false;
      }
    }
  }
}

void updateBubbles()
{
  const uint16_t BUBBLE_END_POSITION = 900;

  for (uint8_t i = 0; i < 2; i++)
  {
    if (!bubbles[i].active)
    {
      continue;
    }

    bubbles[i].position += bubbles[i].speed;

    if (bubbles[i].position >
        BUBBLE_END_POSITION)
    {
      bubbles[i].active = false;
      bubbles[i].position = 0;
    }
  }
}

void renderSingleBubble(const Bubble &bubble)
{
  const uint16_t NODE_SPACING = 256;
  const uint16_t TOP_POSITION = 768;
  const uint16_t END_POSITION = 900;

  if (!bubble.active)
  {
    return;
  }

  /*
    Fade the bubble after it reaches LED1.
  */

  if (bubble.position >= TOP_POSITION)
  {
    uint16_t remaining =
      END_POSITION - bubble.position;

    uint16_t fadeRange =
      END_POSITION - TOP_POSITION;

    uint16_t brightness =
      ((uint32_t)bubble.brightness * remaining) /
      fadeRange;

    addBrightness(
      LED1_INDEX,
      brightness
    );

    return;
  }

  uint8_t lowerNode =
    bubble.position / NODE_SPACING;

  uint8_t upperNode =
    lowerNode + 1;

  uint16_t positionInsideSegment =
    bubble.position % NODE_SPACING;

  uint16_t lowerBrightness =
    ((uint32_t)bubble.brightness *
     (NODE_SPACING - positionInsideSegment)) /
    NODE_SPACING;

  uint16_t upperBrightness =
    ((uint32_t)bubble.brightness *
     positionInsideSegment) /
    NODE_SPACING;

  addBrightness(
    BUBBLE_PATH[lowerNode],
    lowerBrightness
  );

  if (upperNode < BUBBLE_PATH_LENGTH)
  {
    addBrightness(
      BUBBLE_PATH[upperNode],
      upperBrightness
    );
  }
}

void renderBubbles()
{
  for (uint8_t i = 0; i < 2; i++)
  {
    renderSingleBubble(bubbles[i]);
  }
}

/* =========================================================
   LIQUID ANIMATION
   ========================================================= */

void updateLiquidSurge()
{
  if (liquidSurge > LIQUID_SURGE_DECAY)
  {
    liquidSurge -= LIQUID_SURGE_DECAY;
  }
  else
  {
    liquidSurge = 0;
  }
}

uint8_t calculateLiquidCycleScale()
{
  /*
    Slow whole-flask breathing cycle.
  */

  uint8_t cycleWave = triangleWave(
    frameCounter / LIQUID_CYCLE_DIVIDER
  );

  uint16_t cycleRange =
    LIQUID_CYCLE_MAX - LIQUID_CYCLE_MIN;

  return LIQUID_CYCLE_MIN +
    (((uint16_t)cycleWave * cycleRange) / 255);
}

void renderLiquid()
{
  uint8_t liquidCycleScale =
    calculateLiquidCycleScale();

  /*
    Broad waves moving through the flask at different phases.
  */

  int16_t slowWave5 =
    centeredTriangleWave(
      frameCounter + 15
    );

  int16_t slowWave6 =
    centeredTriangleWave(
      frameCounter + 80
    );

  int16_t slowWave7 =
    centeredTriangleWave(
      frameCounter + 145
    );

  int16_t slowWave8 =
    centeredTriangleWave(
      frameCounter + 210
    );

  /*
    Smaller, quicker disturbances layered over the broad wave.
  */

  int16_t ripple5 =
    centeredTriangleWave(
      (frameCounter * 3) + 25
    );

  int16_t ripple6 =
    centeredTriangleWave(
      (frameCounter * 4) + 95
    );

  int16_t ripple7 =
    centeredTriangleWave(
      (frameCounter * 3) + 165
    );

  int16_t ripple8 =
    centeredTriangleWave(
      (frameCounter * 4) + 225
    );

  int16_t led5Raw =
    LIQUID_LED5_BASE +
    ((slowWave5 * LIQUID_SHIMMER_AMOUNT) / 128) +
    ((ripple5 * 5) / 128) +
    liquidSurge;

  int16_t led6Raw =
    LIQUID_LED6_BASE +
    ((slowWave6 * LIQUID_SHIMMER_AMOUNT) / 128) +
    ((ripple6 * 7) / 128) +
    ((liquidSurge * 3) / 4);

  int16_t led7Raw =
    LIQUID_LED7_BASE +
    ((slowWave7 * LIQUID_SHIMMER_AMOUNT) / 128) +
    ((ripple7 * 8) / 128) +
    ((liquidSurge * 2) / 3);

  int16_t led8Raw =
    LIQUID_LED8_BASE +
    ((slowWave8 * LIQUID_SHIMMER_AMOUNT) / 128) +
    ((ripple8 * 6) / 128) +
    (liquidSurge / 2);

  uint8_t led5Brightness =
    clampBrightness(led5Raw);

  uint8_t led6Brightness =
    clampBrightness(led6Raw);

  uint8_t led7Brightness =
    clampBrightness(led7Raw);

  uint8_t led8Brightness =
    clampBrightness(led8Raw);

  /*
    Apply the slow whole-flask brightness cycle.
  */

  ledBrightness[LED5_INDEX] =
    scaleBrightness(
      led5Brightness,
      liquidCycleScale
    );

  ledBrightness[LED6_INDEX] =
    scaleBrightness(
      led6Brightness,
      liquidCycleScale
    );

  ledBrightness[LED7_INDEX] =
    scaleBrightness(
      led7Brightness,
      liquidCycleScale
    );

  ledBrightness[LED8_INDEX] =
    scaleBrightness(
      led8Brightness,
      liquidCycleScale
    );
}

/* =========================================================
   SOFTWARE PWM
   ========================================================= */

void renderPwmFrame()
{
  for (
    uint8_t pwmStep = 0;
    pwmStep < PWM_STEPS;
    pwmStep++
  )
  {
    uint8_t threshold = pwmStep * 8;

    for (
      uint8_t led = 0;
      led < LED_COUNT;
      led++
    )
    {
      bool shouldBeOn =
        ledBrightness[led] > threshold;

      setLedHardware(
        led,
        shouldBeOn
      );
    }

    delayMicroseconds(PWM_SLICE_US);
  }
}

/* =========================================================
   ARDUINO ENTRY POINTS
   ========================================================= */

void setup()
{
  RCC_APB2PeriphClockCmd(
    RCC_APB2Periph_AFIO |
    RCC_APB2Periph_GPIOA |
    RCC_APB2Periph_GPIOC |
    RCC_APB2Periph_GPIOD,
    ENABLE
  );

  disablePA1PA2OscillatorRemap();

  for (uint8_t i = 0; i < LED_COUNT; i++)
  {
    configureOutput(
      LEDS[i].port,
      LEDS[i].pin
    );
  }

  turnAllLedsOff();
  clearBrightness();

  bubbles[0].active = false;
  bubbles[1].active = false;

  scheduleNextMainBubble();

  delay(400);
}

void loop()
{
  clearBrightness();

  updateBubbleScheduler();
  updateBubbles();
  updateLiquidSurge();

  renderLiquid();
  renderBubbles();

  renderPwmFrame();

  frameCounter++;
}
