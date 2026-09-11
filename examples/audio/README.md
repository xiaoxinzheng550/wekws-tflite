# Test audio

All files in this directory are 16 kHz, 16-bit, mono PCM WAV files and can be
passed directly to `kws_main`.

Examples:

```bash
./build/bin/kws_main mfcc 80 256 examples/audio/silence.wav
./build/bin/kws_main mfcc 80 256 examples/audio/wozai.wav
./build/bin/kws_main mfcc 80 256 examples/audio/haode.wav
```

The files include wake-word samples, ordinary speech, silence, and noisy test
recordings. They are provided for reproducibility and technical evaluation;
review their source rights before redistributing them in another product.
