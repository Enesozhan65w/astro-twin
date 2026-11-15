# ASTRO-TWIN

**Autonomous 3D astronaut health digital twin and decision-support system**
Team B.E.K.A. · NASA Space Apps Challenge 2026

On a Mars transit, a message to Earth can take up to 22 minutes each way. ASTRO-TWIN gives the crew a flight surgeon on board: a wearable sensor node, a 3D anatomical digital twin and a triage engine that watches five body systems, flags problems early and tells the crew what to do without waiting for Earth.

## Live demo and slides

- App: open `index.html` in Chrome or Edge, or use the GitHub Pages link of this repository.
- Demo slides (7 slides): [`ASTRO-TWIN_Demo_Slides.pdf`](ASTRO-TWIN_Demo_Slides.pdf)

To stream live data from the hardware, click **Connect ESP32 (USB)**; Web Serial needs Chrome or Edge over HTTPS or a local file.

## How it works

1. **Sense:** an ESP32 reads a surface EMG sensor and a MAX30100 pulse oximeter (MLX90614 infrared thermometer optional) and streams a JSON frame at 20 Hz over USB or through a Raspberry Pi 3 edge bridge. The crew also logs blood pressure, glucose, sleep, mood and the NASA-TLX workload score.
2. **Model:** a Three.js 3D twin shows five body systems, mapped to NASA’s five hazards of human spaceflight, as nominal, caution or critical.
3. **Decide:** a rule-based triage engine returns a readiness score, an EVA go/no-go decision and an escalation level (crew self-care, flight-surgeon alert, autonomous onboard protocol).
4. **Act:** each hazard gets a concrete countermeasure; the ASTRO-AI voice flight surgeon answers questions about the live data offline (optional Google Gemini API key for a cloud model).

## Hardware

ESP32 · surface EMG sensor with electrodes · MAX30100 pulse oximeter · MLX90614 IR thermometer (optional) · Raspberry Pi 3 (edge node) · UNI-T oscilloscope (signal validation).
The ESP32 firmware is in [`esp32_astro_twin.ino`](esp32_astro_twin.ino); it is also embedded in the app (click **ESP32 Code (.ino)** to copy it into the Arduino IDE).

## NASA data and resources

- 5 Hazards of Human Spaceflight: https://www.nasa.gov/hrp/hazards/
- Risks of Human Spaceflight: https://www.nasa.gov/hrp/risks/
- Health Impacts of Radiation in Space and Countermeasures (NTRS 20250004252, Curiosity MSL-RAD): https://ntrs.nasa.gov/api/citations/20250004252/downloads/Book%20Chapter.pdf
- HRP Evidence Report on Sleep Loss, Circadian Desynchronization and Work Overload (NTRS 20150016964): https://ntrs.nasa.gov/api/citations/20150016964/downloads/20150016964.pdf
- NASA-STD-3001 Volume 2: https://standards.nasa.gov/node/237
- NASA-STD-3001 Technical Brief, ECLSS (OCHMO-TB-002): https://www.nasa.gov/wp-content/uploads/2023/07/eclss-technical-brief-ochmo.pdf
- NASA Twins Study: https://www.nasa.gov/humans-in-space/twins-study/
- NASA Open Science Data Repository (OSDR): https://science.nasa.gov/biological-physical/data/osdr/
- Spaceflight Standard Measures: https://www.nasa.gov/reference/standard-measures/
- Artemis II Standard Measures: https://www.nasa.gov/reference/artemis-ii-standard-measures/
- CIPHER: https://www.nasa.gov/reference/cipher/
- NASA Task Load Index (NASA-TLX): https://www.nasa.gov/human-systems-integration-division/nasa-task-load-index-tlx/
- NASA Space Radiation Analysis Group (South Atlantic Anomaly): https://srag.jsc.nasa.gov/spaceradiation/what/what.cfm
- 2023 Moon to Mars Architecture Concept Review, Human Health and Performance: https://www.nasa.gov/wp-content/uploads/2024/01/human-health-and-performance.pdf

Partner agency: ESA, Musculo-skeletal system: Bone and Muscle loss: https://www.esa.int/articles/musculo-skeletal-system-bone-and-muscle-loss

## Open-source libraries

Three.js r128 (MIT) · D3.js 7.9.0 (ISC) · TopoJSON Client 3.1.0 (ISC) · world-atlas / Natural Earth (ISC, public-domain data) · Tailwind CSS 3.4.17 (MIT) · Inter and JetBrains Mono fonts (SIL Open Font License) · Arduino core for ESP32 (LGPL-2.1).

## AI disclosure

- Parts of the Three.js 3D visualization, the oscilloscope renderer, the Web Serial telemetry parser and the ESP32 firmware were written with help from **Google Gemini**. This is also marked in comments at the top of `index.html` and of the firmware.
- The presentation slides and project texts were drafted with **Claude (Anthropic)**.
- At runtime the app can optionally call the Google Gemini API for the voice flight surgeon; without a key it uses a local rule-based assistant.
- No AI-generated images, video or audio are used.
- System design, biomedical threshold mapping and hardware integration: Team B.E.K.A.

## Disclaimer

ASTRO-TWIN is a research and education prototype, not a certified medical device.
