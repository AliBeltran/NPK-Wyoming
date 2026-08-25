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
#include <cstdlib>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace std;


namespace ui {
    const string RESET = "\033[0m";
    const string BOLD = "\033[1m";
    const string DIM = "\033[2m";
    const string GREEN = "\033[38;5;114m";
    const string TEAL = "\033[38;5;80m";
    const string GOLD = "\033[38;5;221m";
    const string PINK = "\033[38;5;218m";
    const string BLUE = "\033[38;5;117m";
    const string RED = "\033[38;5;203m";
    const string WHITE = "\033[97m";
    const string GRAY = "\033[38;5;245m";

    void clear() {
#ifdef _WIN32
        system("cls");
#else
        cout << "\033[2J\033[H";
#endif
    }

    void line(const string& c="─", int n=64) { cout << GRAY; for(int i=0;i<n;++i) cout << c; cout << RESET << "\n"; }

    void title(const string& subtitle="N • P • K DECISION SUPPORT SYSTEM") {
        cout << "\n";
        cout << TEAL << "╭────────────────────────────────────────────────────────────────╮" << RESET << "\n";
        cout << TEAL << "│" << RESET << "  " << BOLD << GREEN << "🌾 WYOMING FERTILITY" << RESET << "                              " << TEAL << "│" << RESET << "\n";
        cout << TEAL << "│" << RESET << "     " << WHITE << subtitle << RESET << "                 " << TEAL << "│" << RESET << "\n";
        cout << TEAL << "│" << RESET << "     " << DIM << "University of Wyoming / NRCS-based decision support" << RESET << "   " << TEAL << "│" << RESET << "\n";
        cout << TEAL << "╰────────────────────────────────────────────────────────────────╯" << RESET << "\n\n";
    }

    void step(int n, const string& name) {
        cout << BOLD << GOLD << "  STEP " << n << " OF 5" << RESET << "  " << DIM << "›" << RESET << "  " << BOLD << WHITE << name << RESET << "\n";
        line();
    }

    void section(const string& name) {
        cout << "\n" << BOLD << GREEN << "  " << name << RESET << "\n";
        line("─", 58);
    }

    string bar(double value, double maxValue, int width=28) {
        if (maxValue <= 0) maxValue = 1;
        double ratio = value / maxValue; if (ratio < 0.0) ratio = 0.0; if (ratio > 1.0) ratio = 1.0;
        int filled = static_cast<int>(ratio * width);
        string out; for(int i=0;i<filled;++i) out += "█"; for(int i=filled;i<width;++i) out += "░"; return out;
    }

    void notice(const string& text) {
        cout << "\n" << GOLD << "  ⚠  " << text << RESET << "\n";
    }

    void success(const string& text) {
        cout << "\n" << GREEN << "  ✓  " << text << RESET << "\n";
    }
}

using ui::RESET; using ui::BOLD; using ui::DIM; using ui::GREEN; using ui::TEAL; using ui::GOLD; using ui::PINK; using ui::BLUE; using ui::RED; using ui::WHITE; using ui::GRAY;

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
    ui::section("SOIL TEXTURE");
    cout << "  " << TEAL << "[1]" << RESET << " Coarse-textured\n";
    cout << "  " << TEAL << "[2]" << RESET << " Medium-textured\n";
    cout << "  " << TEAL << "[3]" << RESET << " Fine-textured / high-lime\n";
    int x = askInt("\n  › Select texture [1-3]: ", 1, 3);
    return static_cast<Texture>(x - 1);
}

Water askWater() {
    ui::section("WATER SUPPLY");
    cout << "  " << BLUE << "[1]" << RESET << " Sufficient / irrigated\n";
    cout << "  " << GOLD << "[2]" << RESET << " Short / limited\n";
    cout << "  " << GRAY << "[3]" << RESET << " Dryland\n";
    int x = askInt("\n  › Select water supply [1-3]: ", 1, 3);
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
    ui::section("NITROGEN CREDIT / CROPPING HISTORY");
    cout << "  " << TEAL << "[1]" << RESET << " None\n";
    cout << "  " << TEAL << "[2]" << RESET << " Beans — subtract 30 lb N/acre\n";
    cout << "  " << TEAL << "[3]" << RESET << " Clean fallow — subtract 20 lb N/acre\n";
    cout << "  " << TEAL << "[4]" << RESET << " Corn stalks — add 20 lb N/ton residue\n";
    cout << "  " << TEAL << "[5]" << RESET << " Forage legume plowed down\n";
    cout << "  " << TEAL << "[6]" << RESET << " Stable manure\n";
    cout << "  " << TEAL << "[7]" << RESET << " Small-grain stubble — add 20 lb N/ton\n";
    cout << "  " << TEAL << "[8]" << RESET << " Current manure — add 5 lb N/ton\n";

    int choice = askInt("\n  › Select adjustment [1-8]: ", 1, 8);
    switch (choice) {
        case 1: return 0;
        case 2: return -30;
        case 3: return -20;
        case 4: return 20.0 * askDouble("  › Tons of corn-stalk residue/acre: ");
        case 5: return 0.8 * askDouble("  › % legume in previous stand: ");
        case 6: return 1.2 * askDouble("  › % legume represented in manure source: ");
        case 7: return 20.0 * askDouble("  › Tons of small-grain stubble/acre: ");
        case 8: return 5.0 * askDouble("  › Tons manure/acre: ");
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
    ui::section("NUTRIENT REQUIREMENT");
    cout << fixed << setprecision(1);
    cout << "\n  " << BOLD << "NITROGEN" << RESET << "              " << BOLD << "PHOSPHORUS" << RESET << "            " << BOLD << "POTASSIUM" << RESET << "\n";
    cout << "  " << GREEN << setw(8) << r.N << RESET << " lb N/acre      "
         << GREEN << setw(8) << r.P2O5 << RESET << " lb P₂O₅/acre      "
         << GREEN << setw(8) << r.K2O << RESET << " lb K₂O/acre\n";
    cout << "\n  N     " << GREEN << ui::bar(r.N, max({r.N,r.P2O5,r.K2O})) << RESET << "\n";
    cout << "  P₂O₅  " << BLUE << ui::bar(r.P2O5, max({r.N,r.P2O5,r.K2O})) << RESET << "\n";
    cout << "  K₂O   " << GOLD << ui::bar(r.K2O, max({r.N,r.P2O5,r.K2O})) << RESET << "\n";

    cout << "\n  " << BOLD << "FERTILIZER-GRADE EQUIVALENT" << RESET << "\n";
    cout << "  " << TEAL << "N - P₂O₅ - K₂O" << RESET << "   "
         << BOLD << round(r.N) << " - " << round(r.P2O5) << " - " << round(r.K2O) << RESET << "\n";
}

void printProducts(const ProductRates& p) {
    ui::section("FERTILIZER PRODUCT PLAN");
    cout << fixed << setprecision(1);
    cout << "\n  " << GREEN << "UREA" << RESET << "  46-0-0     " << setw(10) << p.urea << " lb/acre\n";
    cout << "  " << BLUE << "DAP" << RESET << "   18-46-0    " << setw(10) << p.DAP << " lb/acre\n";
    cout << "  " << GOLD << "KCl" << RESET << "   0-0-62     " << setw(10) << p.KCl << " lb/acre\n";
    cout << "\n";
    ui::line("─", 58);
    cout << "  N supplied       " << setw(10) << p.NFromProducts << " lb/acre\n";
    cout << "  P₂O₅ supplied    " << setw(10) << p.P2O5FromProducts << " lb/acre\n";
    cout << "  K₂O supplied     " << setw(10) << p.K2OFromProducts << " lb/acre\n";
    cout << "\n  " << BOLD << "TOTAL BLEND" << RESET << "        " << BOLD << (p.urea + p.DAP + p.KCl) << " lb fertilizer/acre" << RESET << "\n";
}

Crop askCrop() {
    ui::section("CROP SELECTION");
    struct Item { int n; const char* icon; const char* name; } items[] = {
        {1,"🌱","Established grass / legume"},{2,"🌽","Corn"},{3,"🌾","Millet"},
        {4,"🌾","Barley / oats / wheat"},{5,"🫘","Dry bean"},{6,"🥔","Potato"},
        {7,"🌻","Safflower"},{8,"🥬","Sugarbeet"},{9,"🌻","Sunflower"},
        {10,"🏡","Lawn"},{11,"🌿","Fruit / ornamental / vegetable"}
    };
    for (const auto& i : items) {
        cout << "  " << TEAL << setw(2) << setfill('0') << i.n << setfill(' ') << RESET
             << "  " << i.icon << "  " << i.name << "\n";
    }
    int x = askInt("\n  › Select crop [1-11]: ", 1, 11);
    return static_cast<Crop>(x - 1);
}

int main() {
    cout << fixed << setprecision(1);
    ui::clear();
    ui::title();

    cout << "  " << DIM << "A field-level nutrient planning assistant for Wyoming crops." << RESET << "\n";
    cout << "  " << DIM << "Source: University of Wyoming fertilizer recommendation tables." << RESET << "\n";

    Crop crop = askCrop();

    SoilSample soil;
    Field field;

    ui::clear();
    ui::title("SOIL & FIELD DATA");
    ui::step(2, "SOIL TEST");

    cout << "\n  Enter values from your soil test.\n\n";
    soil.no3N = askDouble("  NO₃-N (ppm)             › ");
    soil.P = askDouble("  Extractable P (ppm)     › ");
    soil.K = askDouble("  Extractable K (ppm)     › ");
    soil.organicMatter = askDouble("  Organic matter (%)      › ");

    ui::step(3, "FIELD CONDITIONS");
    field.texture = askTexture();
    field.water = askWater();

    ui::step(4, "YIELD TARGET");
    if (crop == Crop::EstablishedGrass) {
        field.vegetationGrassPercent = askDouble("\n  Grass percentage of stand (%) › ");
        field.baseYield = 6.0;
        field.yieldGoal = askDouble("  Hay yield goal (tons/acre)     › ");
    } else if (crop == Crop::Corn) {
        field.baseYield = 150.0;
        field.yieldGoal = askDouble("\n  Corn yield goal (bu/acre)      › ");
    } else if (crop == Crop::Millet) {
        field.baseYield = 35.0;
        field.yieldGoal = askDouble("\n  Millet yield goal (bu/acre)    › ");
    } else if (crop == Crop::SmallGrain) {
        field.baseYield = 90.0;
        field.yieldGoal = askDouble("\n  Small-grain yield goal (bu/acre) › ");
    } else if (crop == Crop::DryBean) {
        field.baseYield = 30.0;
        field.yieldGoal = askDouble("\n  Dry-bean yield goal (cwt/acre) › ");
    } else if (crop == Crop::Potato) {
        field.baseYield = 350.0;
        field.yieldGoal = askDouble("\n  Potato yield goal (cwt/acre)   › ");
    } else if (crop == Crop::Safflower) {
        field.baseYield = 1400.0;
        field.yieldGoal = askDouble("\n  Safflower yield goal (lb/acre) › ");
    } else if (crop == Crop::Sugarbeet) {
        field.baseYield = 30.0;
        field.yieldGoal = askDouble("\n  Sugarbeet yield goal (tons/acre) › ");
    } else if (crop == Crop::Sunflower) {
        field.baseYield = 30.0;
        field.yieldGoal = askDouble("\n  Sunflower yield goal (cwt/acre) › ");
    } else {
        field.baseYield = 0.0;
        field.yieldGoal = 0.0;
    }

    Recommendation recommendation = calculateRecommendation(crop, soil, field);

    if (crop == Crop::EstablishedGrass || crop == Crop::Corn ||
        crop == Crop::SmallGrain || crop == Crop::Potato ||
        crop == Crop::Sugarbeet || crop == Crop::Sunflower) {
        if (field.water != Water::Sufficient) {
            ui::notice("The selected crop has water-supply special statements in the Wyoming guide. Review the crop-specific limited-water or dryland statement before field use.");
        }
    }

    if (crop == Crop::EstablishedGrass || crop == Crop::Corn || crop == Crop::SmallGrain) {
        cout << "\n  Apply an N cropping-history/manure adjustment?\n";
        cout << "  " << TEAL << "[1]" << RESET << " Yes     " << GRAY << "[2]" << RESET << " No\n";
        int useHistory = askInt("  › Select [1-2]: ", 1, 2);
        if (useHistory == 1) {
            double adjustment = croppingHistoryNAdjustment();
            recommendation.N = max(0.0, recommendation.N + adjustment);
        }
    }

    ui::clear();
    ui::title("FIELD RECOMMENDATION");
    ui::step(5, "RESULTS");

    ui::section("FIELD PROFILE");
    cout << "  Crop                 " << BOLD << cropName(crop) << RESET << "\n";
    cout << "  Soil texture          " << textureName(field.texture) << "\n";
    cout << "  Water supply          " << waterName(field.water) << "\n";
    cout << "  NO₃-N                 " << soil.no3N << " ppm\n";
    cout << "  Extractable P         " << soil.P << " ppm\n";
    cout << "  Extractable K         " << soil.K << " ppm\n";
    cout << "  Organic matter        " << soil.organicMatter << " %\n";

    printRecommendation(recommendation);

    int products = askInt("\n  Calculate Urea + DAP + KCl application rates? [1=yes, 2=no]: ", 1, 2);
    if (products == 1) {
        ProductRates rates = calculateProducts(recommendation);
        printProducts(rates);
    }

    ui::section("FIELD SUMMARY");
    cout << "  " << GREEN << "✓ Calculation complete" << RESET << "\n";
    cout << "  Crop:       " << cropName(crop) << "\n";
    cout << "  N-P₂O₅-K₂O: " << round(recommendation.N) << "-" << round(recommendation.P2O5) << "-" << round(recommendation.K2O) << "\n";

    ui::notice("This is decision-support software based on the supplied University of Wyoming fertilizer guide. Verify crop, yield goal, water condition, soil-test method, and all special statements before using recommendations in an actual nutrient management plan.");
    cout << "\n  " << DIM << "Not an official NRCS software product." << RESET << "\n\n";
    return 0;
}
