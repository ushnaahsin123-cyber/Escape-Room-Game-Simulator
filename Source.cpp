#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <limits>
#include <iomanip>

using namespace std;

/* ========== Configuration ========== */
const char* ROOMS_TXT = "rooms.txt";
const char* SAVE_BIN = "savegame.dat";
const char* HIGHSCORES_BIN = "highscores.dat";

const int MAX_ROOM_NAME = 128;
const int MAX_DESC = 512;
const int MAX_OBJ_NAME = 128;
const int MAX_ITEM_NAME = 128;
const int MAX_PLAYER_NAME = 32;

/* ========== Global data  ========== */

// Rooms
int totalRooms = 0;
char** roomNames = nullptr;
char** roomDescriptions = nullptr;
int* roomObjectCount = nullptr;
int* roomObjectStart = nullptr;

// Objects (flat arrays)
int totalObjects = 0;
char** objectNames = nullptr;
char** objectDescriptions = nullptr;
int* objectHasPuzzle = nullptr;
int* objectPuzzleType = nullptr;
char** objectPuzzlePrompt = nullptr;
char** objectPuzzleAnswer = nullptr;
int* objectGivesItem = nullptr;
char** objectItemName = nullptr;
int* objectSolved = nullptr;

// Inventory
char** inventory = nullptr;
int invCount = 0;
int invCapacity = 0;

// Runtime state
int currentRoomIndex = 0;
int totalMoves = 0;
int hintsUsed = 0;
time_t startTime = 0;
int accumulatedElapsed = 0;
int difficultyLevel = 2; // 1=Easy,2=Medium,3=Hard

// Last-game summary snapshot (for main menu viewing)
bool lastSummaryAvailable = false;
int lastRoomsExplored = 0;
int lastTotalMoves = 0;
int lastElapsedSec = 0;
int lastHintsUsed = 0;
int lastDifficulty = 2;
int lastFinalScore = 0;
char** lastItemsCollected = nullptr;
int lastItemsCount = 0;
char** lastAchievements = nullptr;
int lastAchievementsCount = 0;

// Quit-to-main control flag
bool exitToMainRequested = false;

/* ========== Utility helpers ========== */

// void clearScreen() {
// #ifdef _WIN32
// 	system("cls");
// #else
// 	cout << "\x1B[2J\x1B[H";
// #endif
// }

void clearScreen() {

}


void pressEnterToContinue() {
	cout << "\nPress Enter to continue...";
	string tmp;
	getline(cin, tmp);
}

string readLineTrimmedFromCin() {
	string s;
	getline(cin, s);
	size_t i = 0;
	while (i < s.size() && isspace((unsigned char)s[i])) ++i;
	size_t j = s.size();
	while (j > i && isspace((unsigned char)s[j - 1])) --j;
	return s.substr(i, j - i);
}

string trimStr(const string &s) {
	size_t st = 0;
	while (st < s.size() && isspace((unsigned char)s[st])) ++st;
	size_t ed = s.size();
	while (ed > st && isspace((unsigned char)s[ed - 1])) --ed;
	return s.substr(st, ed - st);
}

int safeAtoi(const char* s) {
	if (!s) return 0;
	string t(s);
	size_t st = 0; while (st < t.size() && isspace((unsigned char)t[st])) ++st;
	size_t ed = t.size(); while (ed > st && isspace((unsigned char)t[ed - 1])) --ed;
	if (st >= ed) return 0;
	bool neg = false; size_t idx = st;
	if (t[idx] == '-') { neg = true; ++idx; }
	long long v = 0;
	for (; idx < ed; ++idx) {
		if (!isdigit((unsigned char)t[idx])) return 0;
		v = v * 10 + (t[idx] - '0');
		if (v > INT_MAX) break;
	}
	return neg ? -(int)v : (int)v;
}

int readIntInRange(int lo, int hi) {
	string line;
	while (true) {
		line = readLineTrimmedFromCin();
		if (line.size() == 0) {
			cout << "Please enter a number: ";
			continue;
		}
		bool valid = true;
		size_t idx = 0;
		if (line[0] == '-' || line[0] == '+') idx = 1;
		for (; idx < line.size(); ++idx) {
			if (!isdigit((unsigned char)line[idx])) { valid = false; break; }
		}
		if (!valid) {
			cout << "Invalid input. Enter a number: ";
			continue;
		}
		long long v = 0;
		try { v = stoll(line); }
		catch (...) { valid = false; }
		if (!valid || v < lo || v > hi) {
			cout << "Enter number between " << lo << " and " << hi << ": ";
			continue;
		}
		return (int)v;
	}
}

bool readYesNo() {
	while (true) {
		string s = readLineTrimmedFromCin();
		if (s.size() == 0) {
			cout << "Please enter y/n: ";
			continue;
		}
		char c = tolower((unsigned char)s[0]);
		if (c == 'y') return true;
		if (c == 'n') return false;
		cout << "Please enter y/n: ";
	}
}

string toLowerTrim(const string &s) {
	size_t st = 0; while (st < s.size() && isspace((unsigned char)s[st])) ++st;
	size_t ed = s.size(); while (ed > st && isspace((unsigned char)s[ed - 1])) --ed;
	string t = s.substr(st, ed - st);
	for (size_t i = 0; i < t.size(); ++i) t[i] = tolower((unsigned char)t[i]);
	return t;
}

/* ========== Memory / String helpers ========== */

char* allocCstrFromStd(const string &s) {
	size_t n = s.size();
	char* p = new(nothrow) char[n + 1];
	if (!p) { cerr << "Memory allocation failed\n"; exit(1); }
	strcpy_s(p, n + 1, s.c_str());
	return p;
}

char* allocEmptyCstr() {
	char* p = new(nothrow) char[1];
	if (!p) { cerr << "Memory allocation failed\n"; exit(1); }
	p[0] = '\0';
	return p;
}

/* ========== Inventory functions ========== */

void ensureInvCapacity() {
	if (invCapacity == 0) {
		invCapacity = 4;
		inventory = new(nothrow) char*[invCapacity];
		if (!inventory) { cerr << "Memory alloc failed\n"; exit(1); }
	}
	else if (invCount >= invCapacity) {
		int newCap = invCapacity * 2;
		char** tmp = new(nothrow) char*[newCap];
		if (!tmp) { cerr << "Memory alloc failed\n"; exit(1); }
		for (int i = 0; i < invCount; ++i) tmp[i] = inventory[i];
		delete[] inventory;
		inventory = tmp;
		invCapacity = newCap;
	}
}

void addInventory(const char* name) {
	if (!name) return;
	if (strlen(name) == 0) return;
	ensureInvCapacity();
	inventory[invCount] = allocCstrFromStd(string(name));
	++invCount;
	cout << "Added to inventory: " << name << "\n";
}

void showInventory() {
	cout << "Inventory (" << invCount << "):\n";
	if (invCount == 0) { cout << " - Empty -\n"; return; }
	for (int i = 0; i < invCount; ++i) {
		cout << (i + 1) << ". " << inventory[i] << "\n";
	}
}

int findInventoryIndex(const char* name) {
	if (!name) return -1;
	string target = toLowerTrim(string(name));
	for (int i = 0; i < invCount; ++i) {
		if (toLowerTrim(string(inventory[i])) == target) return i;
	}
	return -1;
}

void removeInventoryAt(int idx) {
	if (idx < 0 || idx >= invCount) return;
	delete[] inventory[idx];
	for (int i = idx; i < invCount - 1; ++i) inventory[i] = inventory[i + 1];
	--invCount;
}

/* ========== File helper ========== */

bool fileExists(const char* fname) {
	ifstream f(fname);
	return f.good();
}



void freeAllMemory() {
	// rooms
	if (roomNames) {
		for (int i = 0; i < totalRooms; ++i) delete[] roomNames[i];
		delete[] roomNames; roomNames = nullptr;
	}
	if (roomDescriptions) {
		for (int i = 0; i < totalRooms; ++i) delete[] roomDescriptions[i];
		delete[] roomDescriptions; roomDescriptions = nullptr;
	}
	if (roomObjectCount) { delete[] roomObjectCount; roomObjectCount = nullptr; }
	if (roomObjectStart) { delete[] roomObjectStart; roomObjectStart = nullptr; }

	// objects
	if (objectNames) {
		for (int i = 0; i < totalObjects; ++i) delete[] objectNames[i];
		delete[] objectNames; objectNames = nullptr;
	}
	if (objectDescriptions) {
		for (int i = 0; i < totalObjects; ++i) delete[] objectDescriptions[i];
		delete[] objectDescriptions; objectDescriptions = nullptr;
	}
	if (objectHasPuzzle) { delete[] objectHasPuzzle; objectHasPuzzle = nullptr; }
	if (objectPuzzleType) { delete[] objectPuzzleType; objectPuzzleType = nullptr; }
	if (objectPuzzlePrompt) {
		for (int i = 0; i < totalObjects; ++i) delete[] objectPuzzlePrompt[i];
		delete[] objectPuzzlePrompt; objectPuzzlePrompt = nullptr;
	}
	if (objectPuzzleAnswer) {
		for (int i = 0; i < totalObjects; ++i) delete[] objectPuzzleAnswer[i];
		delete[] objectPuzzleAnswer; objectPuzzleAnswer = nullptr;
	}
	if (objectGivesItem) { delete[] objectGivesItem; objectGivesItem = nullptr; }
	if (objectItemName) {
		for (int i = 0; i < totalObjects; ++i) delete[] objectItemName[i];
		delete[] objectItemName; objectItemName = nullptr;
	}
	if (objectSolved) { delete[] objectSolved; objectSolved = nullptr; }

	// inventory
	if (inventory) {
		for (int i = 0; i < invCount; ++i) delete[] inventory[i];
		delete[] inventory; inventory = nullptr;
	}
	invCount = 0; invCapacity = 0;

	// last summary arrays
	if (lastItemsCollected) {
		for (int i = 0; i < lastItemsCount; ++i) delete[] lastItemsCollected[i];
		delete[] lastItemsCollected; lastItemsCollected = nullptr;
	}
	lastItemsCount = 0;
	if (lastAchievements) {
		for (int i = 0; i < lastAchievementsCount; ++i) delete[] lastAchievements[i];
		delete[] lastAchievements; lastAchievements = nullptr;
	}
	lastAchievementsCount = 0;

	totalRooms = 0;
	totalObjects = 0;
}

bool loadRoomsFromFile(const char* fname) {
	if (!fileExists(fname)) {
		cout << "Rooms file '" << fname << "' not found.\n";
		return false;
	}
	ifstream fin(fname);
	if (!fin) { cout << "Unable to open rooms file.\n"; return false; }

	string line;
	// find first numeric non-empty line -> totalRooms
	int foundTotal = 0;
	while (getline(fin, line)) {
		string t = trimStr(line);
		if (t.size() == 0) continue;
		bool alldigits = true;
		for (size_t i = 0; i < t.size(); ++i) if (!isdigit((unsigned char)t[i])) { alldigits = false; break; }
		if (alldigits) {
			foundTotal = safeAtoi(t.c_str());
			break;
		}
	}
	if (foundTotal <= 0) {
		cout << "Failed to find total rooms count in rooms file.\n";
		fin.close();
		return false;
	}
	totalRooms = foundTotal;

	// allocate room arrays
	roomNames = new(nothrow) char*[totalRooms];
	roomDescriptions = new(nothrow) char*[totalRooms];
	roomObjectCount = new(nothrow) int[totalRooms];
	roomObjectStart = new(nothrow) int[totalRooms];
	if (!roomNames || !roomDescriptions || !roomObjectCount || !roomObjectStart) { cerr << "Memory alloc failed\n"; fin.close(); freeAllMemory(); return false; }
	for (int i = 0; i < totalRooms; ++i) {
		roomNames[i] = allocEmptyCstr();
		roomDescriptions[i] = allocEmptyCstr();
		roomObjectCount[i] = 0;
		roomObjectStart[i] = 0;
	}

	// Temp containers for objects
	int tempCap = 128;
	int tempCount = 0;
	char** tempFields = new(nothrow) char*[tempCap]; // each is array of 7 cstrings
	if (!tempFields) { cerr << "Memory alloc failed\n"; fin.close(); freeAllMemory(); return false; }
	for (int i = 0; i < tempCap; ++i) tempFields[i] = nullptr;
	int* tempRoomIndex = new(nothrow) int[tempCap];
	if (!tempRoomIndex) { cerr << "Memory alloc failed\n"; fin.close(); freeAllMemory(); return false; }

	// rewind and skip first numeric line
	fin.clear();
	fin.seekg(0, ios::beg);
	while (getline(fin, line)) {
		string t = trimStr(line);
		if (t.size() == 0) continue;
		bool alldigits = true;
		for (size_t i = 0; i < t.size(); ++i) if (!isdigit((unsigned char)t[i])) { alldigits = false; break; }
		if (alldigits) break;
	}

	int roomIndex = -1;
	while (getline(fin, line)) {
		string t = trimStr(line);
		if (t.size() == 0) continue;
		if (_stricmp(t.c_str(), "ROOM") == 0) {
			roomIndex++;
			if (roomIndex >= totalRooms) break;
			// read room name
			string rname = "";
			while (getline(fin, line)) {
				rname = trimStr(line);
				if (rname.size() > 0) break;
			}
			if (rname.size() == 0) rname = string("Unnamed Room");
			delete[] roomNames[roomIndex];
			roomNames[roomIndex] = allocCstrFromStd(rname);
			// description
			string rdesc = "";
			while (getline(fin, line)) {
				rdesc = trimStr(line);
				if (rdesc.size() > 0) break;
			}
			if (rdesc.size() == 0) rdesc = string(" ");
			delete[] roomDescriptions[roomIndex];
			roomDescriptions[roomIndex] = allocCstrFromStd(rdesc);

			// OBJECTS n
			int objCount = 0;
			bool gotObjects = false;
			while (getline(fin, line)) {
				string tmp = trimStr(line);
				if (tmp.size() == 0) continue;
				string up = tmp;
				for (size_t i = 0; i < up.size(); ++i) up[i] = toupper((unsigned char)up[i]);
				if (up.rfind("OBJECTS", 0) == 0) {
					size_t pos = tmp.find_first_of("0123456789");
					if (pos != string::npos) {
						string num = tmp.substr(pos);
						objCount = safeAtoi(num.c_str());
					}
					else objCount = 0;
					gotObjects = true;
					break;
				}
				else {
					// keep forgiving: if we encounter "OBJECT" treat it as start (but we expect OBJECTS)
					if (_stricmp(tmp.c_str(), "OBJECT") == 0) {
						gotObjects = true;
						objCount = 0; // we'll just parse until ENDROOM if OBJECTS omitted (edge-case)
						// put the stream pointer back logically is complicated; but Option B uses OBJECTS.
						break;
					}
					else continue;
				}
			}
			if (!gotObjects) objCount = 0;
			roomObjectCount[roomIndex] = objCount;

			// parse objects
			for (int oi = 0; oi < objCount; ++oi) {
				// find OBJECT token
				string tok;
				while (getline(fin, tok)) {
					tok = trimStr(tok);
					if (tok.size() == 0) continue;
					if (_stricmp(tok.c_str(), "OBJECT") == 0) break;
				}
				string oName = "", oDesc = "", pTypeStr = "0", pPrompt = "NOPROMPT", pAnswer = "NOANSWER", givesItemStr = "0", itemNameStr = "NOITEM";
				// name
				while (getline(fin, line)) { oName = trimStr(line); if (oName.size() > 0) break; }
				if (oName.size() == 0) oName = string("Unnamed Object");
				// desc
				while (getline(fin, line)) { oDesc = trimStr(line); if (oDesc.size() > 0) break; }
				if (oDesc.size() == 0) oDesc = string(" ");
				// puzzle type
				while (getline(fin, line)) { pTypeStr = trimStr(line); if (pTypeStr.size() > 0) break; }
				if (pTypeStr.size() == 0) pTypeStr = string("0");
				// prompt
				while (getline(fin, line)) { pPrompt = trimStr(line); if (pPrompt.size() > 0) break; }
				if (pPrompt.size() == 0) pPrompt = string("NOPROMPT");
				// answer
				while (getline(fin, line)) { pAnswer = trimStr(line); if (pAnswer.size() > 0) break; }
				if (pAnswer.size() == 0) pAnswer = string("NOANSWER");
				// gives item
				while (getline(fin, line)) { givesItemStr = trimStr(line); if (givesItemStr.size() > 0) break; }
				if (givesItemStr.size() == 0) givesItemStr = string("0");
				// item name
				while (getline(fin, line)) { itemNameStr = trimStr(line); if (itemNameStr.size() > 0) break; }
				if (itemNameStr.size() == 0) itemNameStr = string("NOITEM");

				if (tempCount >= tempCap) {
					int newCap = tempCap * 2;
					char** nf = new(nothrow) char*[newCap];
					if (!nf) { cerr << "Memory alloc failed\n"; fin.close(); freeAllMemory(); return false; }
					for (int k = 0; k < tempCount; ++k) nf[k] = tempFields[k];
					for (int k = tempCount; k < newCap; ++k) nf[k] = nullptr;
					delete[] tempFields;
					tempFields = nf;
					int* newRooms = new(nothrow) int[newCap];
					if (!newRooms) { cerr << "Memory alloc failed\n"; fin.close(); freeAllMemory(); return false; }
					for (int k = 0; k < tempCount; ++k) newRooms[k] = tempRoomIndex[k];
					delete[] tempRoomIndex;
					tempRoomIndex = newRooms;
					tempCap = newCap;
				}
				char** fields = new(nothrow) char*[7];
				fields[0] = allocCstrFromStd(oName);
				fields[1] = allocCstrFromStd(oDesc);
				fields[2] = allocCstrFromStd(pTypeStr);
				fields[3] = allocCstrFromStd(pPrompt);
				fields[4] = allocCstrFromStd(pAnswer);
				fields[5] = allocCstrFromStd(givesItemStr);
				fields[6] = allocCstrFromStd(itemNameStr);
				tempFields[tempCount] = fields;
				tempRoomIndex[tempCount] = roomIndex;
				++tempCount;
			} // end objects for room
		} // end ROOM token branch
		else {
			// skip unknown
		}
	}
	fin.close();

	// Build final object arrays
	totalObjects = tempCount;
	objectNames = new(nothrow) char*[totalObjects];
	objectDescriptions = new(nothrow) char*[totalObjects];
	objectHasPuzzle = new(nothrow) int[totalObjects];
	objectPuzzleType = new(nothrow) int[totalObjects];
	objectPuzzlePrompt = new(nothrow) char*[totalObjects];
	objectPuzzleAnswer = new(nothrow) char*[totalObjects];
	objectGivesItem = new(nothrow) int[totalObjects];
	objectItemName = new(nothrow) char*[totalObjects];
	objectSolved = new(nothrow) int[totalObjects];

	if (!objectNames || !objectDescriptions || !objectHasPuzzle || !objectPuzzleType || !objectPuzzlePrompt || !objectPuzzleAnswer || !objectGivesItem || !objectItemName || !objectSolved) {
		cerr << "Memory alloc failed\n"; freeAllMemory(); return false;
	}

	// compute room object counts and starts
	for (int r = 0; r < totalRooms; ++r) roomObjectCount[r] = 0;
	for (int i = 0; i < totalObjects; ++i) {
		int ridx = tempRoomIndex[i];
		if (ridx >= 0 && ridx < totalRooms) ++roomObjectCount[ridx];
	}
	int accum = 0;
	for (int r = 0; r < totalRooms; ++r) { roomObjectStart[r] = accum; accum += roomObjectCount[r]; }

	// fill objects into per-room order
	int* cursor = new(nothrow) int[totalRooms];
	for (int r = 0; r < totalRooms; ++r) cursor[r] = 0;
	for (int i = 0; i < totalObjects; ++i) {
		int ridx = tempRoomIndex[i];
		int pos = roomObjectStart[ridx] + cursor[ridx];
		char** f = tempFields[i];
		objectNames[pos] = allocCstrFromStd(string(f[0]));
		objectDescriptions[pos] = allocCstrFromStd(string(f[1]));
		int ptype = safeAtoi(f[2]);
		objectPuzzleType[pos] = ptype;
		objectHasPuzzle[pos] = (ptype != 0) ? 1 : 0;
		string prm = string(f[3]);
		if (_stricmp(prm.c_str(), "NOPROMPT") == 0) prm = string("");
		objectPuzzlePrompt[pos] = allocCstrFromStd(prm);
		string ans = string(f[4]);
		if (_stricmp(ans.c_str(), "NOANSWER") == 0) ans = string("");
		objectPuzzleAnswer[pos] = allocCstrFromStd(ans);
		string gv = string(f[5]);
		int gflag = 0;
		if (_stricmp(gv.c_str(), "1") == 0 || _stricmp(gv.c_str(), "YES") == 0) gflag = 1;
		objectGivesItem[pos] = gflag;
		string iname = string(f[6]);
		if (_stricmp(iname.c_str(), "NOITEM") == 0) iname = string("");
		objectItemName[pos] = allocCstrFromStd(iname);
		objectSolved[pos] = 0;
		++cursor[ridx];
	}
	delete[] cursor;

	// cleanup temp
	for (int i = 0; i < tempCount; ++i) {
		char** ff = tempFields[i];
		for (int k = 0; k < 7; ++k) delete[] ff[k];
		delete[] ff;
	}
	delete[] tempFields;
	delete[] tempRoomIndex;

	cout << "Loaded " << totalRooms << " rooms and " << totalObjects << " objects.\n";
	return true;
}

/* ========== Randomize puzzles ========== */

void randomizePuzzles() {
	srand((unsigned int)time(nullptr));
	for (int r = 0; r < totalRooms; ++r) {
		int start = roomObjectStart[r];
		int cnt = roomObjectCount[r];
		int k = 0;
		int* idx = new(nothrow) int[cnt];
		for (int j = 0; j < cnt; ++j) if (objectHasPuzzle[start + j]) idx[k++] = start + j;
		if (k > 1) {
			for (int i = k - 1; i > 0; --i) {
				int j = rand() % (i + 1);
				int a = idx[i], b = idx[j];
				// swap answers/prompts/types/givesitem/itemname/haspuzzle
				char* ta = objectPuzzleAnswer[a]; objectPuzzleAnswer[a] = objectPuzzleAnswer[b]; objectPuzzleAnswer[b] = ta;
				char* tp = objectPuzzlePrompt[a]; objectPuzzlePrompt[a] = objectPuzzlePrompt[b]; objectPuzzlePrompt[b] = tp;
				int ttype = objectPuzzleType[a]; objectPuzzleType[a] = objectPuzzleType[b]; objectPuzzleType[b] = ttype;
				int thp = objectHasPuzzle[a]; objectHasPuzzle[a] = objectHasPuzzle[b]; objectHasPuzzle[b] = thp;
				int tgi = objectGivesItem[a]; objectGivesItem[a] = objectGivesItem[b]; objectGivesItem[b] = tgi;
				char* ti = objectItemName[a]; objectItemName[a] = objectItemName[b]; objectItemName[b] = ti;
			}
		}
		delete[] idx;
	}
}

/* ========== Puzzle check & inspect ========== */

bool checkPuzzleAnswerByIndex(int objIndex, const string &userInput) {
	if (objIndex < 0 || objIndex >= totalObjects) return false;
	if (!objectHasPuzzle[objIndex]) return true;
	string correct = toLowerTrim(string(objectPuzzleAnswer[objIndex]));
	if (objectPuzzleType[objIndex] == 1) {
		int a = safeAtoi(correct.c_str());
		int b = safeAtoi(userInput.c_str());
		return a == b;
	}
	else {
		return toLowerTrim(userInput) == correct;
	}
}

void inspectObjectInRoom(int roomIdx, int localIndex) {
	int pos = roomObjectStart[roomIdx] + localIndex;
	clearScreen();
	cout << "Inspecting: " << objectNames[pos] << "\n\n";
	cout << objectDescriptions[pos] << "\n\n";
	if (objectSolved[pos]) {
		cout << "(Already solved)\n";
		pressEnterToContinue();
		return;
	}
	if (!objectHasPuzzle[pos]) {
		cout << "No puzzle here.\n";
		if (objectGivesItem[pos] && strlen(objectItemName[pos]) > 0) {
			addInventory(objectItemName[pos]);
			objectSolved[pos] = 1;
		}
		pressEnterToContinue();
		return;
	}
	cout << "Puzzle prompt:\n";
	if (strlen(objectPuzzlePrompt[pos]) > 0) cout << objectPuzzlePrompt[pos] << "\n\n";
	else cout << "(No prompt available)\n\n";
	cout << "Options:\n1. Try to solve\n2. Ask for a hint (penalty)\n3. Back\nChoose: ";
	int choice = readIntInRange(1, 3);
	if (choice == 2) {
		string ans = objectPuzzleAnswer[pos];
		if (objectPuzzleType[pos] == 1) {
			int v = safeAtoi(ans.c_str());
			cout << "HINT: The number is between " << (v / 2) << " and " << (v + 5) << ".\n";
		}
		else {
			string low = toLowerTrim(ans);
			if (low.size() >= 2) cout << "HINT: Starts with '" << low.substr(0, 2) << "'.\n";
			else if (low.size() == 1) cout << "HINT: Starts with '" << low[0] << "'.\n";
			else cout << "HINT: Look around closely.\n";
		}
		++hintsUsed;
		pressEnterToContinue();
		return;
	}
	else if (choice == 3) {
		return;
	}
	cout << "Enter your answer: ";
	string user = readLineTrimmedFromCin();
	if (user.size() == 0) {
		cout << "No answer entered.\n";
		pressEnterToContinue();
		return;
	}
	if (checkPuzzleAnswerByIndex(pos, user)) {
		cout << "Correct! Puzzle solved.\n";
		objectSolved[pos] = 1;
		if (objectGivesItem[pos] && strlen(objectItemName[pos]) > 0) addInventory(objectItemName[pos]);
	}
	else {
		cout << "Incorrect answer. Try again later.\n";
	}
	pressEnterToContinue();
}

/* ========== Room play loop ========== */

void showRoomAscii(int r) {
	cout << "\nRoom Layout (ASCII visual)\n";
	cout << "---------------------------\n";
	cout << "[ Door ]              [ ";
	int cnt = roomObjectCount[r];
	if (cnt > 0) cout << objectNames[roomObjectStart[r]];
	else cout << " ";
	cout << " ]\n";
	if (cnt > 1) cout << "[ Shelf ]          [ " << objectNames[roomObjectStart[r] + 1] << " ]\n";
	cout << "---------------------------\n";
}

int computeScore() {
	int elapsed = accumulatedElapsed + (int)difftime(time(nullptr), startTime);
	int score = 100;
	score -= elapsed / 10;
	score -= hintsUsed * 5;
	score -= totalMoves / 2;
	if (difficultyLevel == 3) score += 10;
	if (difficultyLevel == 1) score -= 5;
	if (score < 0) score = 0;
	if (score > 100) score = 100;
	return score;
}

// forward declarations
void saveProgress();
bool loadProgress();

bool isRoomCompleted(int roomIdx) {
	int start = roomObjectStart[roomIdx];
	int count = roomObjectCount[roomIdx];
	for (int i = 0; i < count; i++) {
		if (!objectSolved[start + i]) {
			return false;
		}
	}
	return true;
}

void playRoomLoop(int r) {
	while (true) {
		clearScreen();
		int elapsed = accumulatedElapsed + (int)difftime(time(nullptr), startTime);
		cout << "Room: " << roomNames[r] << "    Time: " << elapsed << " sec\n";
		showRoomAscii(r);
		cout << "\nObjects:\n";
		int cnt = roomObjectCount[r];
		for (int i = 0; i < cnt; ++i) {
			int pos = roomObjectStart[r] + i;
			cout << (i + 1) << ". " << objectNames[pos];
			if (objectSolved[pos]) cout << " (solved)";
			cout << "\n";
		}
		cout << "I. Inventory\nS. Save Game\nQ. Quit to Main Menu\n";
		cout << "\nEnter choice (number/I/S/Q): ";
		string cmd = readLineTrimmedFromCin();
		if (cmd.size() == 0) continue;
		if ((cmd == "I") || (cmd == "i")) {
			showInventory();
			if (invCount > 0) {
				cout << "Do you want to use an item? (y/n): ";
				if (readYesNo()) {
					cout << "Enter item number to use: ";
					int idx = readIntInRange(1, invCount) - 1;
					if (idx < 0 || idx >= invCount) { cout << "Invalid index.\n"; pressEnterToContinue(); continue; }
					if (roomObjectCount[r] == 0) {
						cout << "No objects in this room to use item on.\n";
						pressEnterToContinue();
						continue;
					}
					cout << "Choose target object to use this on (1-" << roomObjectCount[r] << "): ";
					int targ = readIntInRange(1, roomObjectCount[r]) - 1;
					int pos = roomObjectStart[r] + targ;
					string it = toLowerTrim(string(inventory[idx]));
					string oname = toLowerTrim(string(objectNames[pos]));
					string odesc = toLowerTrim(string(objectDescriptions[pos]));
					bool used = false;
					if (it.find("key") != string::npos && (oname.find("door") != string::npos || oname.find("lock") != string::npos || odesc.find("lock") != string::npos)) {
						cout << "The key fits and unlocks the object!\n";
						objectSolved[pos] = 1;
						used = true;
					}
					else {
						cout << "Using the item had no noticeable effect.\n";
					}
					if (used) {
						cout << "Consume item after use? (y/n): ";
						if (readYesNo()) removeInventoryAt(idx);
					}
				}
			}
			pressEnterToContinue();
			continue;
		}
		else if ((cmd == "S") || (cmd == "s")) {
			saveProgress();
			pressEnterToContinue();
			continue;
		}
		else if ((cmd == "Q") || (cmd == "q")) {
			cout << "Quit to main menu? (y/n): ";
			if (readYesNo()) {
				// signal outer loops to stop and return to main menu
				exitToMainRequested = true;
				return;
			}
			else continue;
		}
		else {
			bool allDigits = true;
			for (size_t i = 0; i < cmd.size(); ++i) if (!isdigit((unsigned char)cmd[i])) { allDigits = false; break; }
			if (!allDigits) { cout << "Invalid command.\n"; pressEnterToContinue(); continue; }
			int num = safeAtoi(cmd.c_str());
			if (num < 1 || num > roomObjectCount[r]) { cout << "Invalid object number.\n"; pressEnterToContinue(); continue; }
			++totalMoves;
			inspectObjectInRoom(r, num - 1);

			// Check if room is completed after solving an object
			if (isRoomCompleted(r)) {
				cout << "\n🎉 All objects in this room are solved! Door unlocked!\n";
				if (r < totalRooms - 1) {
					cout << "Moving to next room...\n";
				}
				else {
					cout << "Final room completed! You've escaped!\n";
				}
				pressEnterToContinue();
				return; // Exit room loop to proceed to next room
			}
		}
	}
}

/* ========== Save / Load binary ========== */

void saveProgress() {
	ofstream fout(SAVE_BIN, ios::binary);
	if (!fout) { cout << "Failed to open save file for writing.\n"; return; }
	fout.write("ESCP1", 5);
	unsigned char version = 1;
	fout.write((char*)&version, 1);
	unsigned char diff = (unsigned char)difficultyLevel;
	fout.write((char*)&diff, 1);
	fout.write((char*)&currentRoomIndex, sizeof(int));
	fout.write((char*)&totalMoves, sizeof(int));
	fout.write((char*)&hintsUsed, sizeof(int));
	int elapsed = accumulatedElapsed + (int)difftime(time(nullptr), startTime);
	fout.write((char*)&elapsed, sizeof(int));
	// inventory
	fout.write((char*)&invCount, sizeof(int));
	for (int i = 0; i < invCount; ++i) {
		int len = (int)strlen(inventory[i]);
		fout.write((char*)&len, sizeof(int));
		if (len > 0) fout.write(inventory[i], len);
	}
	// object flags
	fout.write((char*)&totalObjects, sizeof(int));
	for (int i = 0; i < totalObjects; ++i) {
		unsigned char f = (unsigned char)(objectSolved[i] ? 1 : 0);
		fout.write((char*)&f, 1);
	}
	fout.close();
	cout << "Saving to " << SAVE_BIN << "...\n";
	cout << "✅ Progress saved successfully!\n";
}

bool loadProgress() {
	if (!fileExists(SAVE_BIN)) { cout << "No save file found.\n"; return false; }
	ifstream fin(SAVE_BIN, ios::binary);
	if (!fin) { cout << "Failed to open save file.\n"; return false; }
	char magic[6]; memset(magic, 0, sizeof(magic));
	fin.read(magic, 5);
	if (fin.gcount() != 5 || strncmp(magic, "ESCP1", 5) != 0) { cout << "Invalid or corrupted save file.\n"; fin.close(); return false; }
	unsigned char version = 0; fin.read((char*)&version, 1);
	unsigned char diff = 0; fin.read((char*)&diff, 1);
	int cRoom = 0; fin.read((char*)&cRoom, sizeof(int));
	int tMoves = 0; fin.read((char*)&tMoves, sizeof(int));
	int hUsed = 0; fin.read((char*)&hUsed, sizeof(int));
	int elapsed = 0; fin.read((char*)&elapsed, sizeof(int));
	int inv = 0; fin.read((char*)&inv, sizeof(int));
	// clear current inventory
	for (int i = 0; i < invCount; ++i) delete[] inventory[i];
	delete[] inventory; inventory = nullptr; invCount = 0; invCapacity = 0;
	if (inv < 0) inv = 0;
	invCapacity = inv > 0 ? inv : 4;
	inventory = new(nothrow) char*[invCapacity];
	if (!inventory) { cerr << "Memory alloc failed\n"; exit(1); }
	for (int i = 0; i < inv; ++i) {
		int len = 0; fin.read((char*)&len, sizeof(int));
		if (len <= 0) { inventory[invCount++] = allocEmptyCstr(); continue; }
		char* buf = new(nothrow) char[len + 1];
		if (!buf) { cerr << "Memory alloc failed\n"; exit(1); }
		fin.read(buf, len); buf[len] = 0; inventory[invCount++] = buf;
	}
	int fileTotalObjects = 0; fin.read((char*)&fileTotalObjects, sizeof(int));
	if (fileTotalObjects != totalObjects) { cout << "Save file object count mismatch; cannot load.\n"; fin.close(); return false; }
	for (int i = 0; i < totalObjects; ++i) {
		unsigned char f = 0; fin.read((char*)&f, 1);
		objectSolved[i] = (int)f;
	}
	fin.close();
	difficultyLevel = (int)diff;
	currentRoomIndex = cRoom;
	totalMoves = tMoves;
	hintsUsed = hUsed;
	accumulatedElapsed = elapsed;
	startTime = time(nullptr) - accumulatedElapsed;
	cout << "Save loaded successfully. Resuming from room " << (currentRoomIndex + 1) << ".\n";
	return true;
}

/* ========== High scores ========== */

void appendHighScore(const char* name, int score, int timeSec, int diff, int hints) {
	ofstream fout(HIGHSCORES_BIN, ios::binary | ios::app);
	if (!fout) { cout << "Unable to open high score file for writing.\n"; return; }
	int len = (int)strlen(name);
	fout.write((char*)&len, sizeof(int));
	if (len > 0) fout.write(name, len);
	unsigned char d = (unsigned char)diff;
	fout.write((char*)&d, 1);
	fout.write((char*)&timeSec, sizeof(int));
	fout.write((char*)&score, sizeof(int));
	unsigned short hs = (unsigned short)hints;
	fout.write((char*)&hs, sizeof(unsigned short));
	fout.close();
	cout << "High score recorded.\n";
}

void showHighScores() {
	if (!fileExists(HIGHSCORES_BIN)) { cout << "No high scores yet.\n"; return; }
	ifstream fin(HIGHSCORES_BIN, ios::binary);
	if (!fin) { cout << "Unable to open high score file.\n"; return; }
	fin.seekg(0, ios::end);
	streamoff sz = fin.tellg();
	fin.seekg(0, ios::beg);
	if (sz <= 0) { cout << "No records.\n"; fin.close(); return; }

	int cap = 64; int cnt = 0;
	char** names = new(nothrow) char*[cap];
	unsigned char* diffs = new(nothrow) unsigned char[cap];
	int* timesec = new(nothrow) int[cap];
	int* scores = new(nothrow) int[cap];
	unsigned short* hintsArr = new(nothrow) unsigned short[cap];
	if (!names || !diffs || !timesec || !scores || !hintsArr) { cerr << "Memory alloc failed\n"; fin.close(); return; }

	while (fin.peek() != EOF) {
		int nlen = 0;
		fin.read((char*)&nlen, sizeof(int));
		if (fin.eof()) break;
		char* nbuf = new(nothrow) char[nlen + 1];
		if (!nbuf) { cerr << "Memory alloc failed\n"; break; }
		if (nlen > 0) fin.read(nbuf, nlen);
		nbuf[nlen] = 0;
		unsigned char d = 0; fin.read((char*)&d, 1);
		int t = 0; fin.read((char*)&t, sizeof(int));
		int sc = 0; fin.read((char*)&sc, sizeof(int));
		unsigned short hs = 0; fin.read((char*)&hs, sizeof(unsigned short));
		if (cnt >= cap) {
			int nc = cap * 2;
			char** n1 = new(nothrow) char*[nc];
			unsigned char* n2 = new(nothrow) unsigned char[nc];
			int* n3 = new(nothrow) int[nc];
			int* n4 = new(nothrow) int[nc];
			unsigned short* n5 = new(nothrow) unsigned short[nc];
			for (int i = 0; i < cnt; ++i) { n1[i] = names[i]; n2[i] = diffs[i]; n3[i] = timesec[i]; n4[i] = scores[i]; n5[i] = hintsArr[i]; }
			delete[] names; delete[] diffs; delete[] timesec; delete[] scores; delete[] hintsArr;
			names = n1; diffs = n2; timesec = n3; scores = n4; hintsArr = n5;
			cap = nc;
		}
		names[cnt] = nbuf;
		diffs[cnt] = d;
		timesec[cnt] = t;
		scores[cnt] = sc;
		hintsArr[cnt] = hs;
		++cnt;
	}
	fin.close();
	if (cnt == 0) { cout << "No high scores.\n"; return; }

	// bubble sort by score desc
	for (int i = 0; i < cnt; ++i) {
		for (int j = 0; j < cnt - 1 - i; ++j) {
			if (scores[j] < scores[j + 1]) {
				char* t0 = names[j]; names[j] = names[j + 1]; names[j + 1] = t0;
				unsigned char t1 = diffs[j]; diffs[j] = diffs[j + 1]; diffs[j + 1] = t1;
				int t2 = timesec[j]; timesec[j] = timesec[j + 1]; timesec[j + 1] = t2;
				int t3 = scores[j]; scores[j] = scores[j + 1]; scores[j + 1] = t3;
				unsigned short t4 = hintsArr[j]; hintsArr[j] = hintsArr[j + 1]; hintsArr[j + 1] = t4;
			}
		}
	}

	cout << "===== HIGH SCORES =====\n";
	cout << "# | Player                | Difficulty | Time   | Score | Hints\n";
	cout << "---------------------------------------------------------------\n";
	int limit = cnt < 20 ? cnt : 20;
	for (int i = 0; i < limit; ++i) {
		string dstr = (diffs[i] == 1 ? "Easy" : (diffs[i] == 2 ? "Medium" : "Hard"));
		int mm = timesec[i] / 60; int ss = timesec[i] % 60;
		char timestr[16]; sprintf_s(timestr, "%02d:%02d", mm, ss);
		cout << setw(2) << (i + 1) << " | " << left << setw(21) << names[i] << " | " << setw(9) << dstr << " | " << timestr << " | " << setw(5) << scores[i] << " | " << setw(5) << hintsArr[i] << "\n";
	}

	for (int i = 0; i < cnt; ++i) delete[] names[i];
	delete[] names; delete[] diffs; delete[] timesec; delete[] scores; delete[] hintsArr;
}

/* ========== Achievements & Summary helpers ========== */

int countTotalAssignableItems() {
	int c = 0;
	for (int i = 0; i < totalObjects; ++i) if (objectGivesItem[i] && strlen(objectItemName[i]) > 0) ++c;
	return c;
}

bool inventoryContains(const char* name) {
	if (!name || strlen(name) == 0) return false;
	string t = toLowerTrim(string(name));
	for (int i = 0; i < invCount; ++i) {
		if (toLowerTrim(string(inventory[i])) == t) return true;
	}
	return false;
}

void captureLastGameSummary(int roomsExplored) {
	// free previous snapshot
	if (lastItemsCollected) {
		for (int i = 0; i < lastItemsCount; ++i) delete[] lastItemsCollected[i];
		delete[] lastItemsCollected; lastItemsCollected = nullptr;
	}
	lastItemsCount = 0;
	if (lastAchievements) {
		for (int i = 0; i < lastAchievementsCount; ++i) delete[] lastAchievements[i];
		delete[] lastAchievements; lastAchievements = nullptr;
	}
	lastAchievementsCount = 0;

	lastSummaryAvailable = true;
	lastRoomsExplored = roomsExplored;
	lastTotalMoves = totalMoves;
	lastElapsedSec = accumulatedElapsed + (int)difftime(time(nullptr), startTime);
	lastHintsUsed = hintsUsed;
	lastDifficulty = difficultyLevel;
	lastFinalScore = computeScore();

	// copy items
	if (invCount > 0) {
		lastItemsCollected = new(nothrow) char*[invCount];
		if (!lastItemsCollected) { cerr << "Memory alloc failed\n"; exit(1); }
		for (int i = 0; i < invCount; ++i) lastItemsCollected[i] = allocCstrFromStd(string(inventory[i]));
		lastItemsCount = invCount;
	}
	else {
		lastItemsCollected = nullptr; lastItemsCount = 0;
	}

	// achievements
	int achCap = 8;
	lastAchievements = new(nothrow) char*[achCap];
	lastAchievementsCount = 0;
	if (!lastAchievements) { cerr << "Memory alloc failed\n"; exit(1); }

	// Master Detective: no hints used
	if (lastHintsUsed == 0) {
		lastAchievements[lastAchievementsCount++] = allocCstrFromStd(string("Master Detective (No hints used)"));
	}
	// Speedrunner: under 5 minutes (300 sec)
	if (lastElapsedSec <= 300) lastAchievements[lastAchievementsCount++] = allocCstrFromStd(string("Speedrunner (Under 05:00)"));
	// Collector: collected all assignable items
	int totalAssignable = countTotalAssignableItems();
	int collected = lastItemsCount;
	if (totalAssignable > 0 && collected >= totalAssignable) lastAchievements[lastAchievementsCount++] = allocCstrFromStd(string("Collector (All items collected)"));
	// Perfectionist: score 100/100
	if (lastFinalScore >= 100) lastAchievements[lastAchievementsCount++] = allocCstrFromStd(string("Perfectionist (Perfect score)"));
	// If none, store "None"
	if (lastAchievementsCount == 0) lastAchievements[lastAchievementsCount++] = allocCstrFromStd(string("None"));

	// resize array to actual count (not necessary but okay)
}

/* ========== Final summary display ========== */

void formatTime(int sec, char* out, int outSize) {
	int mm = sec / 60;
	int ss = sec % 60;
	sprintf_s(out, outSize, "%02d:%02d", mm, ss);
}

void showLastGameSummary() {
	if (!lastSummaryAvailable) { cout << "No last-game summary available.\n"; return; }
	clearScreen();
	cout << "===================== GAME SUMMARY =====================\n";
	cout << "Total rooms explored : " << lastRoomsExplored << "\n";
	cout << "Total moves : " << lastTotalMoves << "\n";
	char timestr[32]; formatTime(lastElapsedSec, timestr, sizeof(timestr));
	cout << "Time taken : " << timestr << "\n";
	cout << "Hints used : " << lastHintsUsed << "\n";
	cout << "Items collected : ";
	if (lastItemsCount == 0) cout << "None\n";
	else {
		for (int i = 0; i < lastItemsCount; ++i) {
			cout << lastItemsCollected[i];
			if (i + 1 < lastItemsCount) cout << ", ";
		}
		cout << "\n";
	}
	string dstr = (lastDifficulty == 1 ? "Easy" : (lastDifficulty == 2 ? "Medium" : "Hard"));
	cout << "Difficulty : " << dstr << "\n";
	cout << "Final Score : " << lastFinalScore << "/100\n";
	cout << "--------------------------------------------------------\n";
	cout << "Achievements Unlocked:\n";
	for (int i = 0; i < lastAchievementsCount; ++i) {
		cout << "- " << lastAchievements[i] << "\n";
	}
	cout << "--------------------------------------------------------\n";
}

/* ========== Game flow ========== */

void startNewGame() {
	// reset the quit-to-main flag when a fresh game starts
	exitToMainRequested = false;

	clearScreen();
	cout << "Select difficulty: 1=Easy  2=Medium  3=Hard\nEnter: ";
	int d = readIntInRange(1, 3);
	difficultyLevel = d;
	// reset solved flags
	for (int i = 0; i < totalObjects; ++i) objectSolved[i] = 0;
	// clear inventory
	for (int i = 0; i < invCount; ++i) delete[] inventory[i];
	delete[] inventory; inventory = nullptr; invCount = 0; invCapacity = 0;
	// randomize puzzles
	randomizePuzzles();
	// counters
	currentRoomIndex = 0;
	totalMoves = 0;
	hintsUsed = 0;
	accumulatedElapsed = 0;
	startTime = time(nullptr);

	int roomsExplored = 0;
	for (int r = 0; r < totalRooms; ++r) {
		// if user requested quit to main from prior room, stop immediately
		if (exitToMainRequested) break;
		currentRoomIndex = r;
		playRoomLoop(r);

		// Check if user quit to main menu
		if (exitToMainRequested) break;

		// Check if current room is completed before proceeding
		if (!isRoomCompleted(r)) {
			// Room not completed, don't proceed to next room
			cout << "You need to complete all puzzles in this room before proceeding!\n";
			pressEnterToContinue();
			r--; // Stay in current room
			continue;
		}

		roomsExplored++;
	}

	// if user quit early, just return to main menu without finishing end-of-game flow
	if (exitToMainRequested) {
		cout << "Returning to main menu...\n";
		pressEnterToContinue();
		return;
	}

	// Only show completion message if all rooms were actually completed
	if (roomsExplored == totalRooms) {
		int elapsed = accumulatedElapsed + (int)difftime(time(nullptr), startTime);
		int finalScore = computeScore();
		cout << "\nYou escaped all rooms!\n";
		char timestr[32]; formatTime(elapsed, timestr, sizeof(timestr));
		cout << "Time: " << timestr << " (" << elapsed << " seconds)\n";
		cout << "Moves: " << totalMoves << "   Hints used: " << hintsUsed << "\n";
		cout << "Final score: " << finalScore << "/100\n";

		// capture snapshot for main menu display and achievements
		captureLastGameSummary(roomsExplored);

		// prompt to save progress
		cout << "Do you want to save your progress? (y/n): ";
		if (readYesNo()) {
			saveProgress();
		}

		// highscore entry
		cout << "Enter your name for high score (max " << (MAX_PLAYER_NAME - 1) << " chars): ";
		string pname = readLineTrimmedFromCin();
		if (pname.empty()) pname = "Player";
		if (pname.size() > (MAX_PLAYER_NAME - 1)) pname = pname.substr(0, MAX_PLAYER_NAME - 1);
		appendHighScore(pname.c_str(), finalScore, elapsed, difficultyLevel, hintsUsed);

		cout << "Play again? (y/n): ";
		if (readYesNo()) startNewGame();
		else { cout << "Returning to main menu...\n"; pressEnterToContinue(); }
	}
	else {
		cout << "Game incomplete. Returning to main menu...\n";
		pressEnterToContinue();
	}
}

void resumeSavedGame() {
	// reset quit flag for resumed session
	exitToMainRequested = false;

	if (!loadProgress()) { pressEnterToContinue(); return; }
	// continue
	int roomsExplored = 0;
	for (int r = currentRoomIndex; r < totalRooms; ++r) {
		if (exitToMainRequested) break;
		currentRoomIndex = r;
		playRoomLoop(r);

		// Check if user quit to main menu
		if (exitToMainRequested) break;

		// Check if current room is completed before proceeding
		if (!isRoomCompleted(r)) {
			// Room not completed, don't proceed to next room
			cout << "You need to complete all puzzles in this room before proceeding!\n";
			pressEnterToContinue();
			r--; // Stay in current room
			continue;
		}

		roomsExplored++;
	}
	// if user quit early, just return to main menu
	if (exitToMainRequested) {
		cout << "Returning to main menu...\n";
		pressEnterToContinue();
		return;
	}

	// Only show completion message if all rooms were actually completed
	if (roomsExplored == totalRooms - currentRoomIndex) {
		int elapsed = accumulatedElapsed + (int)difftime(time(nullptr), startTime);
		int finalScore = computeScore();
		cout << "\nYou escaped all rooms!\n";
		char timestr[32]; formatTime(elapsed, timestr, sizeof(timestr));
		cout << "Time: " << timestr << "\n";
		cout << "Moves: " << totalMoves << "   Hints used: " << hintsUsed << "\n";
		cout << "Final score: " << finalScore << "/100\n";

		captureLastGameSummary(roomsExplored);

		cout << "Do you want to save your progress? (y/n): ";
		if (readYesNo()) saveProgress();

		cout << "Enter your name for high score (max " << (MAX_PLAYER_NAME - 1) << " chars): ";
		string pname = readLineTrimmedFromCin();
		if (pname.empty()) pname = "Player";
		if (pname.size() > (MAX_PLAYER_NAME - 1)) pname = pname.substr(0, MAX_PLAYER_NAME - 1);
		appendHighScore(pname.c_str(), finalScore, accumulatedElapsed + (int)difftime(time(nullptr), startTime), difficultyLevel, hintsUsed);
	}
	else {
		cout << "Game incomplete. Progress saved.\n";
	}

	cout << "Returning to main menu...\n";
	pressEnterToContinue();
}



int main() {
	srand((unsigned int)time(nullptr));

	cout << "Loading rooms from '" << ROOMS_TXT << "'...\n";
	if (!loadRoomsFromFile(ROOMS_TXT)) {
		cout << "Failed to load rooms. Make sure '" << ROOMS_TXT << "' exists and is formatted correctly.\n";
		cout << "Press Enter to exit.\n"; getline(cin, *(new string));
		freeAllMemory();
		return 0;
	}

	while (true) {
		clearScreen();
		cout << "=====================================\n";
		cout << "     ESCAPE ROOM SIMULATOR\n";
		cout << "=====================================\n";
		cout << "1. Start New Game\n2. Load Saved Game\n3. View High Scores\n4. View Last Game Summary\n5. Exit\n";
		cout << "Enter choice: ";
		int ch = readIntInRange(1, 5);
		if (ch == 1) startNewGame();
		else if (ch == 2) resumeSavedGame();
		else if (ch == 3) { clearScreen(); showHighScores(); pressEnterToContinue(); }
		else if (ch == 4) { clearScreen(); showLastGameSummary(); pressEnterToContinue(); }
		else { cout << "Goodbye!\n"; break; }
	}

	freeAllMemory();
	system("pause");
	return 0;
}