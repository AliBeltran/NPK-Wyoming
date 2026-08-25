/*
 Wyoming N-P-K Fertility Calculator
 ----------------------------------
 Source basis:
   University of Wyoming, Guide to Wyoming Fertilizer Recommendations
   Publication B-1045 / USDA-NRCS Agronomy Technical Note No. 10 (2009)

 This is a decision-support implementation of the tables in the supplied guide.
 It is NOT an official NRCS software product.

 Features:
   - Multiple Wyoming crop recommendation tables
   - Soil-test N, P, and K inputs
   - Soil texture selection
   - Organic-matter input where applicable
   - Yield-goal adjustments
   - Irrigation / water-supply selection
   - Selected cropping-history and manure N credits
   - Urea, DAP, and KCl fertilizer-product calculations
   - Final nutrient recommendation and product application rates

 Nutrient units:
   Soil tests: ppm unless noted
   Recommendations: lb/acre
   P and K recommendations are expressed as P2O5 and K2O.
*/

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace std;

enum class Texture { Coarse, Medium, Fine };
enum class Water { Sufficient, Short, Dryland };
enum class Crop {
    EstablishedGrass,
    Corn,
    Millet,
    SmallGrain,
    DryBean,
    Potato,
    Safflower,
    Sugarbeet,
    Sunflower,
    Lawn,
    Horticulture
};

struct Recommendation {
    double N = 0.0;
    double P2O5 = 0.0;
    double K2O = 0.0;
};

struct SoilSample {
    double no3N = 0.0;
    double organicMatter = 0.0;
    double P = 0.0;
    double K = 0.0;
};

struct Field {
    double yieldGoal = 0.0;
    double baseYield = 0.0;
    double vegetationGrassPercent = 100.0;
    Texture texture = Texture::Medium;
    Water water = Water::Sufficient;
};

struct ProductRates {
    double urea = 0.0;   // 46-0-0
    double DAP = 0.0;    // 18-46-0
    double KCl = 0.0;    // 0-0-62
    double NFromProducts = 0.0;
    double P2O5FromProducts = 0.0;
    double K2OFromProducts = 0.0;
};

constexpr double UREA_N = 0.46;
constexpr double DAP_N = 0.18;
constexpr double DAP_P2O5 = 0.46;
constexpr double KCL_K2O = 0.62;

double clamp(double x, double lo, double hi) {
    return max(lo, min(hi, x));
}

int category(double x, const vector<double>& upperBounds) {
    for (size_t i = 0; i < upperBounds.size(); ++i)
        if (x <= upperBounds[i]) return static_cast<int>(i);
    return static_cast<int>(upperBounds.size());
}

string textureName(Texture t) {
    switch (t) {
        case Texture::Coarse: return "Coarse-textured";
        case Texture::Medium: return "Medium-textured";
        case Texture::Fine: return "Fine-textured / high-lime";
    }
    return "Unknown";
}

string waterName(Water w) {
    switch (w) {
        case Water::Sufficient: return "Sufficient";
        case Water::Short: return "Short";
        case Water::Dryland: return "Dryland";
    }
    return "Unknown";
}

string cropName(Crop c) {
    switch (c) {
        case Crop::EstablishedGrass: return "Established grass / legume";
        case Crop::Corn: return "Corn";
        case Crop::Millet: return "Millet";
        case Crop::SmallGrain: return "Barley / oats / wheat";
        case Crop::DryBean: return "Dry bean";
        case Crop::Potato: return "Potato";
        case Crop::Safflower: return "Safflower";
        case Crop::Sugarbeet: return "Sugarbeet";
        case Crop::Sunflower: return "Sunflower";
        case Crop::Lawn: return "Lawn";
        case Crop::Horticulture: return "Fruit / ornamental / vegetable";
    }
    return "Unknown";
}

int askInt(const string& prompt, int minValue, int maxValue) {
    int value;
    while (true) {
        cout << prompt;
        if (cin >> value && value >= minValue && value <= maxValue) return value;
        cout << "Please enter a value from " << minValue << " to " << maxValue << ".\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

double askDouble(const string& prompt, double minValue = 0.0) {
    double value;
    while (true) {
        cout << prompt;
        if (cin >> value && value >= minValue) return value;
        cout << "Please enter a value >= " << minValue << ".\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

Texture askTexture() {
    cout << "\nSoil texture:\n"
         << "  1. Coarse-textured\n"
         << "  2. Medium-textured\n"
         << "  3. Fine-textured / high-lime\n";
    int x = askInt("Select: ", 1, 3);
    return static_cast<Texture>(x - 1);
}

Water askWater() {
    cout << "\nWater supply:\n"
         << "  1. Sufficient\n"
         << "  2. Short / limited\n"
         << "  3. Dryland\n";
    int x = askInt("Select: ", 1, 3);
    return static_cast<Water>(x - 1);
}

/*
 Table 5, established grasses/legumes.
 Surface sample, 0-1 ft.
 Values shown for vegetation percentages:
 100, 80, 60, 40, 20, 0.

 This table is used for a 6-ton/acre hay yield goal.
*/
double grassN[10][6] = {
    {250, 200, 150, 100, 50, 0},
    {230, 180, 130,  80, 30, 0},
    {210, 160, 110,  60,  0, 0},
    {190, 140,  90,  40,  0, 0},
    {170, 120,  70,  20,  0, 0},
    {150, 100,  50,   0,  0, 0},
    {130,  80,  30,   0,  0, 0},
    {110,  60,   0,   0,  0, 0},
    { 90,  40,   0,   0,  0, 0},
    { 70,   0,   0,   0,  0, 0}
};

/* Table 6, established grasses/legumes: P2O5, lb/acre. */
double grassP[4][3] = {
    {55, 75, 100},  // 0-6 ppm
    {20, 40,  70},  // 7-14
    { 0,  0,  40},  // 15-22
    { 0,  0,   0}   // >22
};

/* Table 7, established grasses/legumes: K2O, lb/acre. */
double grassK[6] = {210, 160, 110, 78, 34, 0};

/* Corn: Table 8 N, Table 9 P2O5, Table 10 K2O. */
double cornN[6][6] = {
    {255, 245, 235, 225, 215, 205},
    {195, 185, 175, 165, 155, 145},
    {165, 155, 145, 135, 125, 115},
    {135, 125, 115, 105,  95,  85},
    {105,  95,  85,  75,  65,  55},
    { 75,  65,  55,  45,  35,  25}
};
/*
 Corn N uses organic matter columns:
 0-0.5, 0.6-1.0, 1.1-1.5, 1.6-2.0, >2.0.
 The supplied table also has a 0-6 through >48 NO3-N axis.
 This implementation uses the five OM columns above.
*/
double cornP[4][3] = {
    {80,  65,  90},
    {45,  45,  90},
    { 0,  30,  55},
    { 0,   0,   0}
};
double cornK[7] = {222, 185, 149, 113, 71, 24, 0};

/* Millet: Tables 11-13. */
double milletN[5][5] = {
    {70, 60, 50, 40, 30},
    {50, 40, 30, 20, 10},
    {30, 20, 10,  0,  0},
    {10,  0,  0,  0,  0},
    { 0,  0,  0,  0,  0}
};
double milletP[4][3] = {
    {40, 40, 60},
    { 0, 20, 40},
    { 0,  0, 20},
    { 0,  0,  0}
};
double milletK[2] = {30, 0};

/* Barley/oats/wheat: Tables 14-16. */
double smallGrainN[8][5] = {
    {140,130,120,110,100},
    {120,110,100, 90, 80},
    {100, 90, 80, 70, 60},
    { 80, 70, 60, 50, 40},
    { 60, 50, 40, 30, 20},
    { 40, 30, 20, 10,  0},
    { 20, 10,  0,  0,  0},
    {  0,  0,  0,  0,  0}
};
double smallGrainP[4][3] = {
    {20, 40, 45},
    { 0, 20, 25},
    { 0,  0,  0},
    { 0,  0,  0}
};
double smallGrainK[6] = {162,125,89,54,20,0};

/* Dry bean: Tables 17-19. */
double dryBeanN[5] = {90, 68, 45, 23, 0};
double dryBeanP[4][3] = {
    {45, 60, 55},
    { 0, 30, 50},
    { 0,  0, 20},
    { 0,  0,  0}
};
double dryBeanK[3] = {48, 24, 0};

/* Potato: Tables 20-22. */
double potatoN[8][5] = {
    {160,150,140,130,120},
    {150,140,130,120,110},
    {140,130,120,110,100},
    {130,120,110,100, 90},
    {120,110,100, 90, 80},
    {110,100, 90, 80, 70},
    {100, 90, 80, 70, 60},
    {110,100, 90, 80, 70} // >54 row in supplied table
};
double potatoP[4][3] = {
    {60, 80,105},
    {30, 50, 75},
    { 0, 20, 45},
    { 0,  0,  0}
};
double potatoK[7] = {312,226,179,131,83,35,0};

/* Safflower: Tables 23-25. */
double safflowerN[5] = {65,45,25,0,0};
double safflowerP[4][3] = {
    {20,28,48},
    { 0,14,36},
    { 0,  0,16},
    { 0,  0,  0}
};
double safflowerK[3] = {62,43,24};

/* Sugarbeet: Tables 26-28. */
double sugarbeetN[6][5] = {
    {255,245,235,225,215},
    {215,205,195,185,175},
    {180,170,160,150,140},
    {150,140,130,120,110},
    {120,110,100, 90, 80},
    { 95, 85, 75, 65, 55}
};
double sugarbeetP[4][3] = {
    {90,110,135},
    {60, 80,105},
    {30, 50, 75},
    { 0,  0, 45}
};
double sugarbeetK[7] = {228,158,107,59,35,0,0};

/* Sunflower: Tables 29-31. */
double sunflowerN[8][5] = {
    {165,155,145,135,125},
    {145,135,125,115,105},
    {125,115,105, 95, 85},
    {105, 95, 85, 75, 65},
    { 85, 75, 65, 55, 45},
    { 65, 55, 45, 35, 25},
    { 45, 35, 25, 15,  5},
    { 25, 15,  5,  0,  0}
};
double sunflowerP[4][3] = {
    {55, 75,100},
    {35, 55, 80},
    { 0, 35, 60},
    { 0,  0, 40}
};
double sunflowerK[3] = {60,30,0};

/* Lawn: Tables 32-34. */
double lawnN[6] = {4,3,2,1,0,0};
double lawnP[4][3] = {
    {3.0, 2.0, 3.0},
    {1.5, 1.5, 2.0},
    {1.0, 1.0, 1.5},
    {0.5, 0.5, 1.0}
};
double lawnK[3] = {2,1,0};

/* Horticulture: Tables 32-34. */
double horticultureN[7] = {3.0, 2.5, 2.0, 1.5, 1.0, 0.5, 0.0};
double horticultureP[4][3] = {
    {2.0, 2.5, 3.0},
    {1.5, 2.0, 2.5},
    {1.0, 1.5, 2.0},
    {0.5, 1.0, 1.5}
};
double horticultureK[3] = {2,1,0};

/*
 Generic P lookup for the tables.
 */
double lookupP(double soilP, Texture texture, double table[][3]) {
    int row;
    if (soilP <= 6) row = 0;
    else if (soilP <= 14) row = 1;
    else if (soilP <= 22) row = 2;
    else row = 3;

    int col = static_cast<int>(texture);
    return table[row][col];
}

double lookupK(double soilK, const double table[], int size) {
    int row;
    if (soilK <= 30) row = 0;
    else if (soilK <= 60) row = 1;
    else if (soilK <= 90) row = 2;
    else if (soilK <= 120) row = 3;
    else if (soilK <= 150) row = 4;
    else row = 5;

    row = min(row, size - 1);
    return table[row];
}

double interpolateVegetationN(double no3, double grassPercent) {
    int nRow;
    if (no3 <= 6) nRow = 0;
    else if (no3 <= 12) nRow = 1;
    else if (no3 <= 18) nRow = 2;
    else if (no3 <= 24) nRow = 3;
    else if (no3 <= 30) nRow = 4;
    else if (no3 <= 36) nRow = 5;
    else if (no3 <= 42) nRow = 6;
    else if (no3 <= 48) nRow = 7;
    else if (no3 <= 54) nRow = 8;
    else nRow = 9;

    const double pct[6] = {100,80,60,40,20,0};
    grassPercent = clamp(grassPercent, 0, 100);

    for (int i = 0; i < 5; ++i) {
        if (grassPercent <= pct[i] && grassPercent >= pct[i+1]) {
            double f = (grassPercent - pct[i+1]) / (pct[i] - pct[i+1]);
            return grassN[nRow][i+1] +
                   f * (grassN[nRow][i] - grassN[nRow][i+1]);
        }
    }
    return grassN[nRow][0];
}

/*
 Organic matter column:
 0 = 0-0.5
 1 = 0.6-1.0
 2 = 1.1-1.5
 3 = 1.6-2.0
 4 = >2.0
 */
int omIndex(double om) {
    if (om <= 0.5) return 0;
    if (om <= 1.0) return 1;
    if (om <= 1.5) return 2;
    if (om <= 2.0) return 3;
    return 4;
}

double yieldAdjusted(double base, double yieldGoal, double baseYield,
                     double changePerUnit) {
    return max(0.0, base + (yieldGoal - baseYield) * changePerUnit);
}

double waterAdjusted(double value, Water water, double shortFactor) {
    if (water == Water::Sufficient) return value;
    if (water == Water::Short) return value * shortFactor;
    return value * shortFactor;
}

/*
 Cropping-history N credit / subtraction based on Table 2.
 This is optional and should only be used when the selected
 history actually applies.
 */
double croppingHistoryNAdjustment() {
    cout << "\nCropping-history / manure N adjustment:\n";
    cout << "  1. None\n";
    cout << "  2. Beans: subtract 30 lb N/acre\n";
    cout << "  3. Clean fallow: subtract 20 lb N/acre\n";
    cout << "  4. Corn stalks: add 20 lb N per ton residue\n";
    cout << "  5. Forage legume plowed down: enter % legume\n";
    cout << "  6. Stable manure: add 1.2 lb N x % legume\n";
    cout << "  7. Small-grain stubble: add 20 lb N per ton residue\n";
    cout << "  8. Current manure: add 5 lb N per ton\n";

    int choice = askInt("Select: ", 1, 8);

    switch (choice) {
        case 1: return 0;
        case 2: return -30;
        case 3: return -20;
        case 4: {
            double tons = askDouble("Tons of corn-stalk residue/acre: ");
            return 20.0 * tons;
        }
        case 5: {
            double pct = askDouble("% legume in previous stand: ");
            return 0.8 * pct;
        }
        case 6: {
            double pct = askDouble("% legume represented in manure source: ");
            return 1.2 * pct;
        }
        case 7: {
            double tons = askDouble("Tons of small-grain stubble/acre: ");
            return 20.0 * tons;
        }
        case 8: {
            double tons = askDouble("Tons manure/acre: ");
            return 5.0 * tons;
        }
    }
    return 0;
}

/*
 Fertilizer products:
   DAP supplies P2O5 first and contributes N.
   KCl supplies K2O.
   Urea supplies remaining N.

 This is a simple three-product blend calculator. It does not
 optimize cost or account for sulfur or micronutrients.
 */
ProductRates calculateProducts(const Recommendation& rec) {
    ProductRates p;

    p.DAP = rec.P2O5 / DAP_P2O5;
    double nFromDAP = p.DAP * DAP_N;

    p.KCl = rec.K2O / KCL_K2O;

    double remainingN = max(0.0, rec.N - nFromDAP);
    p.urea = remainingN / UREA_N;

    p.NFromProducts = p.urea * UREA_N + nFromDAP;
    p.P2O5FromProducts = p.DAP * DAP_P2O5;
    p.K2OFromProducts = p.KCl * KCL_K2O;

    return p;
}

Recommendation calculateEstablishedGrass(const SoilSample& s, const Field& f) {
    Recommendation r;
    r.N = interpolateVegetationN(s.no3N, f.vegetationGrassPercent);
    r.P2O5 = lookupP(s.P, f.texture, grassP);
    r.K2O = lookupK(s.K, grassK, 6);

    // Tables 5-7 are based on a 6 ton/acre hay goal.
    // Special statement: adjust N/P2O5/K2O by 40/15/40 per ton.
    r.N = yieldAdjusted(r.N, f.yieldGoal, 6.0, 40.0);
    r.P2O5 = yieldAdjusted(r.P2O5, f.yieldGoal, 6.0, 15.0);
    r.K2O = yieldAdjusted(r.K2O, f.yieldGoal, 6.0, 40.0);

    return r;
}

Recommendation calculateCorn(const SoilSample& s, const Field& f) {
    Recommendation r;

    int no3Row = category(s.no3N, {6,12,18,24,30,36,42,48});
    no3Row = min(no3Row, 5);
    int om = omIndex(s.organicMatter);
    r.N = cornN[no3Row][min(om,5)];

    r.P2O5 = lookupP(s.P, f.texture, cornP);
    r.K2O = lookupK(s.K, cornK, 7);

    // Corn table base yield is 150 bu/acre.
    // Guide: add/subtract 1.6 lb N per bushel change;
    // P2O5 3 lb and K2O 1.2 lb per bushel change.
    double delta = f.yieldGoal - 150.0;
    r.N = max(0.0, r.N + delta * 1.6);
    r.P2O5 = max(0.0, r.P2O5 + delta * 3.0);
    r.K2O = max(0.0, r.K2O + delta * 1.2);

    return r;
}

Recommendation calculateMillet(const SoilSample& s, const Field& f) {
    Recommendation r;

    int n = category(s.no3N, {6,12,18,24});
    n = min(n, 4);
    int om = omIndex(s.organicMatter);
    r.N = milletN[n][min(om,4)];
    r.P2O5 = lookupP(s.P, f.texture, milletP);
    r.K2O = (s.K <= 60 ? 30.0 : 0.0);

    // Base yield 35 bu/acre; +/- 2 N, 0.6 P2O5, 1.75 K2O per bu.
    double delta = f.yieldGoal - 35.0;
    r.N = max(0.0, r.N + delta * 2.0);
    r.P2O5 = max(0.0, r.P2O5 + delta * 0.6);
    r.K2O = max(0.0, r.K2O + delta * 1.75);

    return r;
}

Recommendation calculateSmallGrain(const SoilSample& s, const Field& f) {
    Recommendation r;

    int n = category(s.no3N, {6,12,18,24,30,36,42});
    n = min(n, 7);
    int om = omIndex(s.organicMatter);
    r.N = smallGrainN[n][min(om,4)];
    r.P2O5 = lookupP(s.P, f.texture, smallGrainP);
    r.K2O = lookupK(s.K, smallGrainK, 6);

    // Use wheat base yield of 90 bu/acre as the general module.
    double delta = f.yieldGoal - 90.0;
    r.N = max(0.0, r.N + delta * 1.72);
    r.P2O5 = max(0.0, r.P2O5 + delta * 1.0);
    r.K2O = max(0.0, r.K2O + delta * 2.0);

    return r;
}

Recommendation calculateDryBean(const SoilSample& s, const Field& f) {
    Recommendation r;
    int n = category(s.no3N, {6,10,15,20});
    n = min(n, 4);
    r.N = dryBeanN[n];
    r.P2O5 = lookupP(s.P, f.texture, dryBeanP);
    r.K2O = lookupK(s.K, dryBeanK, 3);

    // Base yield 30 cwt/acre.
    double delta = f.yieldGoal - 30.0;
    r.P2O5 = max(0.0, r.P2O5 + delta * 1.75);
    r.K2O = max(0.0, r.K2O + delta * 4.0);

    return r;
}

Recommendation calculatePotato(const SoilSample& s, const Field& f) {
    Recommendation r;
    int n = category(s.no3N, {18,24,30,36,42,48,54});
    n = min(n, 7);
    int om = omIndex(s.organicMatter);
    r.N = potatoN[n][min(om,4)];
    r.P2O5 = lookupP(s.P, f.texture, potatoP);
    r.K2O = lookupK(s.K, potatoK, 7);

    // Base yield 350 cwt/acre.
    double delta = f.yieldGoal - 350.0;
    r.N = max(0.0, r.N + delta * 0.5);
    r.P2O5 = max(0.0, r.P2O5 + delta * 0.2);
    r.K2O = max(0.0, r.K2O + delta * 1.0);

    return r;
}

Recommendation calculateSafflower(const SoilSample& s, const Field& f) {
    Recommendation r;
    int n = category(s.no3N, {6,12,18});
    n = min(n, 4);
    r.N = safflowerN[n];
    r.P2O5 = lookupP(s.P, f.texture, safflowerP);
    r.K2O = lookupK(s.K, safflowerK, 3);

    // Base yield 1400 lb/acre.
    double deltaHundred = (f.yieldGoal - 1400.0) / 100.0;
    r.N = max(0.0, r.N + deltaHundred * 5.0);
    r.P2O5 = max(0.0, r.P2O5 + deltaHundred * 2.0);
    r.K2O = max(0.0, r.K2O + deltaHundred * 2.0);

    return r;
}

Recommendation calculateSugarbeet(const SoilSample& s, const Field& f) {
    Recommendation r;
    int n = category(s.no3N, {6,12,18,24,30});
    n = min(n, 5);
    int om = omIndex(s.organicMatter);
    r.N = sugarbeetN[n][min(om,4)];
    r.P2O5 = lookupP(s.P, f.texture, sugarbeetP);
    r.K2O = lookupK(s.K, sugarbeetK, 7);

    // Base yield 30 tons/acre; +/- 9 N, 3 P2O5, 9 K2O per ton.
    double delta = f.yieldGoal - 30.0;
    r.N = max(0.0, r.N + delta * 9.0);
    r.P2O5 = max(0.0, r.P2O5 + delta * 3.0);
    r.K2O = max(0.0, r.K2O + delta * 9.0);

    return r;
}

Recommendation calculateSunflower(const SoilSample& s, const Field& f) {
    Recommendation r;
    int n = category(s.no3N, {6,12,18,24,30,36,42});
    n = min(n, 7);
    int om = omIndex(s.organicMatter);
    r.N = sunflowerN[n][min(om,4)];
    r.P2O5 = lookupP(s.P, f.texture, sunflowerP);
    r.K2O = lookupK(s.K, sunflowerK, 3);

    // Base yield 30 cwt/acre; +/- 6 N, 2 P2O5, 2 K2O per cwt.
    double delta = f.yieldGoal - 30.0;
    r.N = max(0.0, r.N + delta * 6.0);
    r.P2O5 = max(0.0, r.P2O5 + delta * 2.0);
    r.K2O = max(0.0, r.K2O + delta * 2.0);

    return r;
}

Recommendation calculateLawn(const SoilSample& s, const Field& f) {
    Recommendation r;

    int n = category(s.no3N, {15,31,47,62});
    n = min(n, 5);
    r.N = lawnN[n];

    int pRow = s.P <= 6 ? 0 : s.P <= 14 ? 1 : s.P <= 22 ? 2 : 3;
    r.P2O5 = lawnP[pRow][static_cast<int>(f.texture)];

    r.K2O = s.K <= 60 ? 2.0 : s.K <= 120 ? 1.0 : 0.0;

    return r;
}

Recommendation calculateHorticulture(const SoilSample& s, const Field& f) {
    Recommendation r;

    int n = category(s.no3N, {15,31,48,63,79,95});
    n = min(n, 6);
    r.N = horticultureN[n];

    int pRow = s.P <= 6 ? 0 : s.P <= 14 ? 1 : s.P <= 22 ? 2 : 3;
    r.P2O5 = horticultureP[pRow][static_cast<int>(f.texture)];

    r.K2O = s.K <= 60 ? 2.0 : s.K <= 120 ? 1.0 : 0.0;

    return r;
}

Recommendation calculateRecommendation(Crop crop,
                                        const SoilSample& s,
                                        const Field& f) {
    switch (crop) {
        case Crop::EstablishedGrass: return calculateEstablishedGrass(s, f);
        case Crop::Corn: return calculateCorn(s, f);
        case Crop::Millet: return calculateMillet(s, f);
        case Crop::SmallGrain: return calculateSmallGrain(s, f);
        case Crop::DryBean: return calculateDryBean(s, f);
        case Crop::Potato: return calculatePotato(s, f);
        case Crop::Safflower: return calculateSafflower(s, f);
        case Crop::Sugarbeet: return calculateSugarbeet(s, f);
        case Crop::Sunflower: return calculateSunflower(s, f);
        case Crop::Lawn: return calculateLawn(s, f);
        case Crop::Horticulture: return calculateHorticulture(s, f);
    }
    return {};
}

void printRecommendation(const Recommendation& r) {
    cout << "\n========================================\n";
    cout << " NUTRIENT RECOMMENDATION\n";
    cout << "========================================\n";
    cout << fixed << setprecision(1);
    cout << "N     : " << r.N << " lb/acre\n";
    cout << "P2O5  : " << r.P2O5 << " lb/acre\n";
    cout << "K2O   : " << r.K2O << " lb/acre\n";

    cout << "\nFertilizer-grade equivalent:\n";
    cout << "N-P2O5-K2O = "
         << round(r.N) << "-"
         << round(r.P2O5) << "-"
         << round(r.K2O) << "\n";
}

void printProducts(const ProductRates& p) {
    cout << "\n========================================\n";
    cout << " FERTILIZER PRODUCT CALCULATION\n";
    cout << "========================================\n";

    cout << fixed << setprecision(1);
    cout << "Urea (46-0-0): " << p.urea << " lb/acre\n";
    cout << "DAP  (18-46-0): " << p.DAP << " lb/acre\n";
    cout << "KCl  (0-0-62): " << p.KCl << " lb/acre\n";

    cout << "\nN supplied:    " << p.NFromProducts << " lb/acre\n";
    cout << "P2O5 supplied: " << p.P2O5FromProducts << " lb/acre\n";
    cout << "K2O supplied:  " << p.K2OFromProducts << " lb/acre\n";

    cout << "\nApproximate total blend:\n"
         << p.urea + p.DAP + p.KCl << " lb fertilizer/acre\n";
}

Crop askCrop() {
    cout << "\n========================================\n";
    cout << " WYOMING FERTILITY CALCULATOR\n";
    cout << "========================================\n";
    cout << " 1. Established grass / legume\n";
    cout << " 2. Corn\n";
    cout << " 3. Millet\n";
    cout << " 4. Barley / oats / wheat\n";
    cout << " 5. Dry bean\n";
    cout << " 6. Potato\n";
    cout << " 7. Safflower\n";
    cout << " 8. Sugarbeet\n";
    cout << " 9. Sunflower\n";
    cout << "10. Lawn\n";
    cout << "11. Fruit / ornamental / vegetable\n";

    int x = askInt("Select crop: ", 1, 11);
    return static_cast<Crop>(x - 1);
}

int main() {
    cout << fixed << setprecision(1);

    Crop crop = askCrop();

    SoilSample soil;
    Field field;

    cout << "\n--- SOIL SAMPLE ---\n";
    soil.no3N = askDouble("NO3-N (ppm): ");
    soil.P = askDouble("Extractable P (ppm): ");
    soil.K = askDouble("Extractable K (ppm): ");

    /*
      Organic matter is used by several crop tables.
      It is still collected for all crops so the program can
      consistently represent a complete soil sample.
    */
    soil.organicMatter = askDouble("Organic matter (%): ");

    field.texture = askTexture();
    field.water = askWater();

    cout << "\n--- FIELD INFORMATION ---\n";

    if (crop == Crop::EstablishedGrass) {
        field.vegetationGrassPercent =
            askDouble("Grass percentage of stand (%): ");
        field.baseYield = 6.0;
        field.yieldGoal =
            askDouble("Hay yield goal (tons/acre): ");
    }
    else if (crop == Crop::Corn) {
        field.baseYield = 150.0;
        field.yieldGoal =
            askDouble("Corn yield goal (bu/acre): ");
    }
    else if (crop == Crop::Millet) {
        field.baseYield = 35.0;
        field.yieldGoal =
            askDouble("Millet yield goal (bu/acre): ");
    }
    else if (crop == Crop::SmallGrain) {
        field.baseYield = 90.0;
        field.yieldGoal =
            askDouble("Small-grain yield goal (bu/acre): ");
    }
    else if (crop == Crop::DryBean) {
        field.baseYield = 30.0;
        field.yieldGoal =
            askDouble("Dry-bean yield goal (cwt/acre): ");
    }
    else if (crop == Crop::Potato) {
        field.baseYield = 350.0;
        field.yieldGoal =
            askDouble("Potato yield goal (cwt/acre): ");
    }
    else if (crop == Crop::Safflower) {
        field.baseYield = 1400.0;
        field.yieldGoal =
            askDouble("Safflower yield goal (lb/acre): ");
    }
    else if (crop == Crop::Sugarbeet) {
        field.baseYield = 30.0;
        field.yieldGoal =
            askDouble("Sugarbeet yield goal (tons/acre): ");
    }
    else if (crop == Crop::Sunflower) {
        field.baseYield = 30.0;
        field.yieldGoal =
            askDouble("Sunflower yield goal (cwt/acre): ");
    }
    else {
        field.baseYield = 0.0;
        field.yieldGoal = 0.0;
    }

    Recommendation recommendation =
        calculateRecommendation(crop, soil, field);

    /*
      Water-supply adjustments are intentionally conservative here.
      The supplied guide's water-supply section describes "short"
      and "dryland" conditions and provides crop-specific special
      statements. The exact adjustment differs by crop and situation,
      so this implementation only applies the explicit crop-table
      adjustments already encoded above rather than inventing a
      universal percentage reduction.
    */

    if (crop == Crop::EstablishedGrass ||
        crop == Crop::Corn ||
        crop == Crop::SmallGrain ||
        crop == Crop::Potato ||
        crop == Crop::Sugarbeet ||
        crop == Crop::Sunflower) {

        if (field.water != Water::Sufficient) {
            cout << "\nNOTICE: The selected crop has water-supply "
                    "special statements in the Wyoming guide.\n";
            cout << "Review the crop-specific limited-water or "
                    "dryland statement before field use.\n";
        }
    }

    /*
      Optional cropping-history credit.
      This is primarily intended for situations where the Wyoming
      guide's N adjustment table applies.
    */
    if (crop == Crop::EstablishedGrass ||
        crop == Crop::Corn ||
        crop == Crop::SmallGrain) {

        int useHistory = askInt(
            "\nApply an N cropping-history/manure adjustment? "
            "(1=yes, 2=no): ", 1, 2);

        if (useHistory == 1) {
            double adjustment = croppingHistoryNAdjustment();
            recommendation.N =
                max(0.0, recommendation.N + adjustment);
        }
    }

    printRecommendation(recommendation);

    int products = askInt(
        "\nCalculate urea + DAP + KCl application rates? "
        "(1=yes, 2=no): ", 1, 2);

    if (products == 1) {
        ProductRates rates =
            calculateProducts(recommendation);
        printProducts(rates);
    }

    cout << "\n========================================\n";
    cout << " SUMMARY\n";
    cout << "========================================\n";
    cout << "Crop: " << cropName(crop) << "\n";
    cout << "Texture: " << textureName(field.texture) << "\n";
    cout << "Water supply: " << waterName(field.water) << "\n";

    cout << "\nThis calculator is based on the supplied\n";
    cout << "University of Wyoming fertilizer guide.\n";
    cout << "Verify the crop, yield goal, water condition,\n";
    cout << "soil-test method, and all special statements\n";
    cout << "before using recommendations in an actual\n";
    cout << "nutrient management plan.\n";

    return 0;
}
