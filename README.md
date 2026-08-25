# 🌱 Wyoming N-P-K Fertility Calculator

A C++ soil fertility decision-support tool that converts soil-test and field information into **N, P₂O₅, and K₂O fertilizer recommendations** using the University of Wyoming Guide to Fertilizer Recommendations.

## Features

* Interactive terminal interface
* Multiple crop recommendation modules
* Soil-test NO₃-N, P, K, and organic matter inputs
* Soil texture and water-supply selection
* Yield-goal adjustments
* Optional cropping-history/manure N credits
* Urea, DAP, and KCl application-rate calculations
* Built-in sample soil-test scenario
* Final N-P₂O₅-K₂O recommendation

## Run the Program

### macOS / Linux

Compile:

```bash
g++ -std=c++17 wyoming_fertility_calculator.cpp -o fertility_calculator
```

Run:

```bash
./fertility_calculator
```

### Windows

With MinGW:

```bash
g++ -std=c++17 wyoming_fertility_calculator.cpp -o fertility_calculator.exe
```

Run:

```bash
fertility_calculator.exe
```

## Sample Soil-Test Scenario

The program includes a built-in example for an **established irrigated grass field**.

| Soil / Field Input |       Value |
| ------------------ | ----------: |
| NO₃-N              |      18 ppm |
| Organic Matter     |        2.0% |
| Soil P             |      17 ppm |
| Soil K             |      90 ppm |
| Soil Texture       |      Medium |
| Grass              |        100% |
| Yield Goal         | 6 tons/acre |

From the main menu, select:

```text
2. Run sample soil-test scenario
```

The calculator will process the sample and display the resulting N, P₂O₅, and K₂O recommendations.

## Interactive Workflow

```text
WYOMING N-P-K FERTILITY CALCULATOR

1. New fertility recommendation
2. Run sample soil-test scenario
3. About this project
4. Exit
```

A new recommendation follows this workflow:

```text
Crop
  ↓
Soil Test
  ↓
Soil Texture
  ↓
Water Supply
  ↓
Yield Goal
  ↓
N-P₂O₅-K₂O Recommendation
  ↓
Urea + DAP + KCl Rates
```

## Purpose

This project combines **agronomy, soil science, and software development** to demonstrate how agricultural fertilizer recommendation tables can be translated into an interactive computational tool.

The goal is to create a practical example of how agricultural data can be used within a programming environment to support nutrient-management decisions.

## Source

**University of Wyoming Guide to Fertilizer Recommendations, Publication B-1045**

The guide was transmitted through **USDA-NRCS Agronomy Technical Note No. 10, November 2009**, and is identified as the fertilizer recommendation source for nutrient management planning.

## Disclaimer

This is an **educational and portfolio project**, not official NRCS software.

Fertilizer recommendations should be verified against the applicable crop table, soil-test method, yield goal, water condition, and crop-specific guidance before being used for an actual nutrient management plan.

