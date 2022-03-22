#include "burner.h"
#include "../logic/NetCode.h"
#include "../logic/utils/fmt/format.h"
#include "../logic/utils/string/StringConvert.h"

const int MAXPLAYER = 4;
static int nPlayerInputs[MAXPLAYER], nCommonInputs, nDIPInputs;
static int nPlayerOffset[MAXPLAYER], nCommonOffset, nDIPOffset;

const int INPUTSIZE = 8 * (4 + 8);
static unsigned char nControls[INPUTSIZE];

// Inputs are assumed to be in the following order:
// All player 1 controls
// All player 2 controls (if any)
// All player 3 controls (if any)
// All player 4 controls (if any)
// All common controls
// All DIP switches

int NetworkInitInput()
{
	if (nGameInpCount == 0) {
		return 1;
	}

	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));

	unsigned int i = 0;

	nPlayerOffset[0] = 0;
	do {
		BurnDrvGetInputInfo(&bii, i);
		i++;
	} while (!_strnicmp(bii.szName, "P1", 2) && i <= nGameInpCount);
	i--;
	nPlayerInputs[0] = i - nPlayerOffset[0];

	for (int j = 1; j < MAXPLAYER; j++) {
		char szString[3] = "P?";
		szString[1] = j + '1';
		nPlayerOffset[j] = i;
		while (!_strnicmp(bii.szName, szString, 2) && i < nGameInpCount) {
			i++;
			BurnDrvGetInputInfo(&bii, i);
		}
		nPlayerInputs[j] = i - nPlayerOffset[j];
	}

	nCommonOffset = i;
	while ((bii.nType & BIT_GROUP_CONSTANT) == 0 && i < nGameInpCount){
		i++;
		BurnDrvGetInputInfo(&bii, i);
	};
	nCommonInputs = i - nCommonOffset;

	nDIPOffset = i;
	nDIPInputs = nGameInpCount - nDIPOffset;

//#if defined FBA_DEBUG
#if 1
	dprintf(_T("  * Network inputs configured as follows --\n"));
	for (int j = 0; j < MAXPLAYER; j++) {
		dprintf(_T("    p%d offset %d, inputs %d.\n"), j + 1, nPlayerOffset[j], nPlayerInputs[j]);
	}
	dprintf(_T("    common offset %d, inputs %d.\n"), nCommonOffset, nCommonInputs);
	dprintf(_T("    dip offset %d, inputs %d.\n"), nDIPOffset, nDIPInputs);
#endif

	return 0;
}

int NetworkGetInput(bool syncOnly)
{
	int i, j, k;
	std::string inputName;
	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));

	// Initialize controls to 0
	memset(nControls, 0, INPUTSIZE);


	// Pack all DIP switches + common controls + player 1 controls
	for (i = 0, j = 0; i < nPlayerInputs[0]; i++, j++) {
		BurnDrvGetInputInfo(&bii, i + nPlayerOffset[0]);
		if (*bii.pVal && bii.nType == BIT_DIGITAL) {
			nControls[j >> 3] |= (1 << (j & 7));

			inputName += bii.szName;
			inputName += ",";
		}
	}
	inputName = "";
	for (i = 0; i < nCommonInputs; i++, j++) {
		BurnDrvGetInputInfo(&bii, i + nCommonOffset);
		bool allow_reset = !strcmp(BurnDrvGetTextA(DRV_NAME), "sf2hf") && kNetVersion >= NET_VERSION_RESET_SF2HF;
		bool can_tilt = !kNetGame || strcmp(bii.szName, "Tilt");
		bool can_reset = !kNetGame || VidOverlayCanReset() || (strcmp(bii.szName, "Reset") && strcmp(bii.szName, "Diagnostic") && strcmp(bii.szName, "Service") && strcmp(bii.szName, "Test")) || allow_reset;
		if (*bii.pVal && can_tilt && can_reset) {
			nControls[j >> 3] |= (1 << (j & 7));
		}
	}

	// Convert j to byte count
	j = (j + 7) >> 3;

	// Analog controls/constants
	for (i = 0; i < nPlayerInputs[0]; i++) {
		BurnDrvGetInputInfo(&bii, i + nPlayerOffset[0]);
		if (*bii.pVal && bii.nType != BIT_DIGITAL) {
			if (bii.nType & BIT_GROUP_ANALOG) {
				nControls[j++] = *bii.pShortVal >> 8;
				nControls[j++] = *bii.pShortVal & 0xFF;
			} else {
				nControls[j++] = *bii.pVal;
			}
		}
	}

	// DIP switches
	for (i = 0; i < nDIPInputs; i++, j++) {
		BurnDrvGetInputInfo(&bii, i + nDIPOffset);
		nControls[j] = *bii.pVal;
	}

	// k has the size of all inputs for one player
	k = j + 1;

	// Send the control block to the Network DLL & retrieve all controls
	if (kNetGame) {
		//if (!QuarkGetInput(nControls, k, MAXPLAYER)) {
		//	return 1;
		//}

		if (NetCodeManager::GetInstance()->getNetInput(nControls, k, 2) == false)
		{
			return 1;
		}
	}

	// Ö´ÐÐÖ¡
	auto local = fmt::format("local [{} {} {} {} {} {} {} {} {}]", 
		nControls[0],
		nControls[1],
		nControls[2],
		nControls[3],
		nControls[4],
		nControls[5],
		nControls[6],
		nControls[7],
		nControls[8]);

	auto remote = fmt::format("remote [{} {} {} {} {} {} {} {} {}]",
		nControls[9 + 0],
		nControls[9 + 1],
		nControls[9 + 2],
		nControls[9 + 3],
		nControls[9 + 4],
		nControls[9 + 5],
		nControls[9 + 6],
		nControls[9 + 7],
		nControls[9 + 8]);

	auto frameId = NetCodeManager::GetInstance()->getFrameId();
	NetCodeManager::GetInstance()->sendLog(L"exec", fmt::format(L"Ö´ÐÐframe:{}", frameId));
	NetCodeManager::GetInstance()->sendLog(L"exec", a2w(local));
	NetCodeManager::GetInstance()->sendLog(L"exec", a2w(remote));

	// Decode Player 1 input block
	for (i = 0, j = 0; i < nPlayerInputs[0]; i++, j++) {
		BurnDrvGetInputInfo(&bii, i + nPlayerOffset[0]);
		if (bii.nType == BIT_DIGITAL) {
			if (nControls[j >> 3] & (1 << (j & 7))) {
				*bii.pVal = 0x01;

				inputName += bii.szName;
				inputName += ",";
			} else {
				*bii.pVal = 0x00;
			}
		}
	}
	for (i = 0; i < nCommonInputs; i++, j++) {
		BurnDrvGetInputInfo(&bii, i + nCommonOffset);
		if (nControls[j >> 3] & (1 << (j & 7))) {
			*bii.pVal = 0x01;
		} else {
			*bii.pVal = 0x00;
		}
	}

	// Convert j to byte count
	j = (j + 7) >> 3;

	// Analog inputs
	for (i = 0; i < nPlayerInputs[0]; i++) {
		BurnDrvGetInputInfo(&bii, i + nDIPOffset);
		if (bii.nType & BIT_GROUP_ANALOG) {
			*bii.pShortVal = (nControls[j] << 8) | nControls[j + 1];
			j += 2;
		}
	}

	// DIP switches
	for (i = 0; i < nDIPInputs; i++, j++) {
		BurnDrvGetInputInfo(&bii, i + nDIPOffset);
		*bii.pVal = nControls[j];
	}

   // Decode other player's input blocks
	for (int l = 1; l < MAXPLAYER; l++) {
		if (nPlayerInputs[l]) {
			for (i = 0, j = k * (l << 3); i < nPlayerInputs[l]; i++, j++) {
				BurnDrvGetInputInfo(&bii, i + nPlayerOffset[l]);
				if (bii.nType == BIT_DIGITAL) {
					if (nControls[j >> 3] & (1 << (j & 7))) {
						*bii.pVal = 0x01;

						inputName += bii.szName;
						inputName += ",";
					} else {
						*bii.pVal = 0x00;
					}
				}
			}

			for (i = 0; i < nCommonInputs; i++, j++) {
#if 0
				// Allow other players to use common inputs
				BurnDrvGetInputInfo(&bii, i + nCommonOffset);
				if (nControls[j >> 3] & (1 << (j & 7))) {
					*bii.pVal |= 0x01;
				}
#endif
			}

			// Convert j to byte count
			j = (j + 7) >> 3;

			// Analog inputs/constants
			for (i = 0; i < nPlayerInputs[l]; i++) {
				BurnDrvGetInputInfo(&bii, i + nPlayerOffset[l]);
				if (bii.nType != BIT_DIGITAL) {
					if (bii.nType & BIT_GROUP_ANALOG) {
						*bii.pShortVal = (nControls[j] << 8) | nControls[j + 1];
						j += 2;
					}
				}
			}

// TEST if this is needed for both players?
#if 1
			// For a DIP switch to be set to 1, ALL players must set it
			for (i = 0; i < nDIPInputs; i++, j++) {
				BurnDrvGetInputInfo(&bii, i + nDIPOffset);
				*bii.pVal &= nControls[j];
			}
#endif
		}
	}

	NetCodeManager::GetInstance()->sendLog(L"exec", a2w(inputName));
	
	return 0;
}
