# Climate Station v0.1 — Future Implementations

## Future Roadmap & Scalability Strategy

To further enhance the technical capabilities, measurement accuracy, and operational efficiency of this IoT Weather Station, the following upgrades are planned for future iterations.

---

### 1. Hardware & Sensor Array Enhancements

#### High-Precision Thermo-Hygrometric Measurement

- **Upgrade:** Transitioning from DHT11 sensors to the **Youmile SHT31 (2.5V–5V)** sensor module.
- **Engineering Rationale:** Offers significantly higher precision ($\pm 2\%$ RH, $\pm 0.2^\circ\text{C}$ accuracy), faster response times, and I2C digital interface support for more reliable microclimate tracking.
- **Implementation notes:**
  - The SHT31 communicates over I2C (default address `0x44`, alternate `0x45`), so it can share the existing bus with the SSD1306 OLED (`SDA = GPIO 21`, `SCL = GPIO 22`) without requiring extra GPIOs.
  - Since the DHT11's single-wire timing-sensitive protocol is replaced by I2C, GPIO 4 and GPIO 5 (currently used by the twin DHT11 sensors) would be freed up for other digital peripherals.
  - Twin-sensor redundancy can be preserved by wiring two SHT31 modules at different I2C addresses (`0x44` / `0x45`, selectable via the `ADDR` pin), keeping the existing MAE/RMSE cross-validation logic in the data science pipeline with minimal changes.
  - Firmware migration requires replacing the `DHT.h` library calls with the `Adafruit_SHT31` (or equivalent) library, and updating the JSON payload field names/units if resolution changes.

#### Air Quality & Gas Detection System

- **Upgrade:** Integrating the **MQ Sensor Series (MQ-2, MQ-3, MQ-4, MQ-135)**.
- **Engineering Rationale:** Expands the station into an environmental air quality monitoring node, capable of detecting LPG, smoke, methane ($CH_4$), alcohol, and volatile organic compounds (VOCs).
- **Implementation notes:**
  - MQ-series sensors output an analog voltage proportional to gas concentration, requiring additional **ADC1-only** input pins (e.g., GPIO 32/33/36/39) to avoid conflicts with Wi-Fi's ADC2 restriction — the same constraint already respected by the current LDR wiring on GPIO 34/35.
  - These sensors require a heater warm-up period (typically 24–48h for stable baseline readings) and benefit from the same voltage-divider and ADC-saturation-filtering approach already implemented for the LDRs (values ≥ 4095 → `NaN` + linear interpolation).
  - Calibration curves (Rs/Ro ratio vs. ppm) must be derived per gas type and documented alongside the firmware, since raw ADC values are not directly interpretable as concentration units.
  - New Firestore fields (e.g., `mq2_ppm`, `mq135_ppm`) would need to be added to the `weather_readings` schema, with corresponding updates to Firestore security rules and the data science notebook's ingestion/loading cells.

#### Barometric Pressure & Altitude Estimation

- **Upgrade:** Incorporating the **BMP280 (3.3V)** digital barometric pressure sensor.
- **Engineering Rationale:** Enables precise atmospheric pressure monitoring (hPa), enabling short-term weather forecasting models and barometric altitude estimation via I2C/SPI buses.
- **Implementation notes:**
  - Like the SHT31, the BMP280 can share the existing I2C bus (default address `0x76` or `0x77`), avoiding additional wiring complexity.
  - Pressure readings open the door to deriving altitude via the barometric formula and to short-term trend-based forecasting (e.g., falling pressure correlating with incoming precipitation), which would extend the "Derived thermodynamic indices" section of the notebook (currently dew point and thermal rate of change).
  - Power budget and I2C bus timing should be re-validated once three or more I2C devices share the same bus, to avoid clock-stretching conflicts with the OLED refresh rate.

---

### 2. Backend & Data Pipeline Automation

#### Automated Cloud ETL Pipelines

- Implementation of scheduled serverless jobs (e.g., Google Cloud Functions, AWS Lambda, or GitHub Actions workflows) to periodically ingest, clean, and run statistical anomaly detection algorithms automatically.
- **Implementation notes:**
  - A scheduled function (e.g., Cloud Scheduler + Cloud Functions, running hourly or daily) would replace the current manual/notebook-triggered extraction, automatically exporting fresh `weather_raw_data.csv` / `weather_clean_data.csv` snapshots into cloud storage (e.g., a Cloud Storage bucket), keeping `data/climate_station_v0.1/` as a local mirror for offline analysis.
  - The existing quality-control steps (packet loss rate, ADC saturation filtering, twin-sensor MAE/RMSE validation, Z-score anomaly detection) already implemented in the notebook are good candidates to be refactored into a shared, importable Python module so both the scheduled ETL job and the notebook reuse the same validated logic instead of duplicating it.
  - All automated jobs must use least-privilege service accounts (read-only Firestore access for extraction, write-only access scoped to the target storage bucket) and secrets (API keys, service account credentials) must be stored in a secrets manager rather than committed to the repository.

#### CI/CD & MLOps Integration

- Deployment of automated testing for incoming sensor payload schemas and continuous evaluation of time-series predictive models.
- **Implementation notes:**
  - Schema validation (e.g., using `pydantic` or Firestore security rules with field-type/range constraints) should reject malformed payloads at ingestion time, preventing corrupted records from reaching `weather_readings`.
  - A CI pipeline (GitHub Actions) could run notebook execution checks (`nbconvert --execute` or `papermill`) on every push, verifying the pipeline still runs end-to-end against a fixture dataset before merging changes.
  - If predictive models (e.g., short-term temperature/pressure forecasting) are introduced, their evaluation metrics (MAE, RMSE, drift over time) should be tracked release-over-release to detect model degradation.

---

### 3. Web Dashboard & Advanced Analytics

#### Interactive Web Visualization

- Development of an enhanced web interface featuring interactive real-time telemetry (using Grafana, Streamlit, or a custom React frontend).
- **Implementation notes:**
  - A lightweight option is a Streamlit app reading directly from Firestore for near-real-time charts; a more scalable option is a custom frontend (e.g., React + Firebase Hosting) backed by Firestore's real-time listeners.
  - Any public-facing dashboard must enforce read-only, rate-limited access to Firestore (via security rules or a backend proxy) to prevent data exfiltration or quota-exhaustion abuse, since the ingestion endpoint already requires an `x-api-key` header.

#### Extended Statistical Metrics

- Inclusion of rolling moving averages, trend decomposition, anomaly confidence scores, and historical comparative heatmaps to provide deeper contextual insights into long-term climate behavior.
- **Implementation notes:**
  - Rolling averages and basic anomaly detection (Z-score) are already present in the current notebook; this item extends them with trend/seasonal decomposition (e.g., `statsmodels`' STL decomposition) and probabilistic anomaly confidence scores instead of a hard Z-score threshold.
  - Comparative heatmaps (e.g., hour-of-day vs. day-of-week) would reuse the existing hourly-profile and photoperiod-detection logic already implemented in the pipeline.

---

## Security & Review Notes

As part of this roadmap entry, the project was reviewed for safety before documenting these plans:

- No credentials, API keys, or service account files are committed anywhere in the repository; the notebook only references a placeholder (`path_to_credentials`) and currently loads data from the local CSV snapshots instead of a live connection.
- The ingestion endpoint already requires an `x-api-key` header — any new sensors (MQ-series, BMP280, SHT31) added under this roadmap must reuse the same authenticated ingestion path rather than introducing new unauthenticated endpoints.
- Analog additions (MQ-series) must stay on ADC1-only pins, consistent with the existing LDR wiring, to avoid conflicts with Wi-Fi's ADC2 restriction.
- Any future automation (scheduled ETL, CI/CD) must use least-privilege, scoped service accounts and store secrets outside of version control.
- This document contains no executable code and only describes planned hardware/software upgrades; it does not alter the current v0.1 firmware, backend, or notebook behavior.
