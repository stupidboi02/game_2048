#include <gui/screen2_screen/Screen2View.hpp>
#include <touchgfx/Unicode.hpp>
#include <cmsis_os.h>
#include "main.h"
#include <math.h>
#include <string.h>
#include <touchgfx/Color.hpp>
#include <gui/common/FrontendApplication.hpp> // Để chuyển màn hình

extern osMessageQueueId_t myQueue01Handle;
extern int highScore;

#define FLASH_SECTOR_SAVE      FLASH_SECTOR_11
#define FLASH_SECTOR_ADDRESS   0x080F0000
#define FLASH_GAME_STATE_ADDRESS    0x080F0400 // Địa chỉ mới cho GameState

Screen2View::Screen2View()
{
	 :buttonCallback(this, &Screen2View::buttonCallbackHandler),
	 isGamePaused(false) // Khởi tạo trạng thái game không tạm dừng
}
colortype getColorForValue(int value)
{
    switch (value)
    {
    case 2:    return Color::getColorFromRGB(238, 228, 218);
    case 4:    return Color::getColorFromRGB(237, 224, 200);
    case 8:    return Color::getColorFromRGB(242, 177, 121);
    case 16:   return Color::getColorFromRGB(245, 149, 99);
    case 32:   return Color::getColorFromRGB(246, 124, 95);
    case 64:   return Color::getColorFromRGB(246, 94, 59);
    case 128:  return Color::getColorFromRGB(237, 207, 114);
    case 256:  return Color::getColorFromRGB(237, 204, 97);
    case 512:  return Color::getColorFromRGB(237, 200, 80);
    case 1024: return Color::getColorFromRGB(237, 197, 63);
    case 2048: return Color::getColorFromRGB(237, 194, 46);
    default:   return Color::getColorFromRGB(205, 193, 180);
    }
}
void Screen2View::saveHighScoreToFlash()
{
    HAL_FLASH_Unlock();

    // Xóa sector trước khi ghi
    FLASH_EraseInitTypeDef erase;
    uint32_t sectorError;
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;  // 2.7V - 3.6V
    erase.Sector       = FLASH_SECTOR_SAVE;
    erase.NbSectors    = 1;

    if (HAL_FLASHEx_Erase(&erase, &sectorError) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return;
    }

    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_SECTOR_ADDRESS, highScore);

    HAL_FLASH_Lock();
}

void Screen2View::loadHighScoreFromFlash()
{
    uint32_t value = *(uint32_t*)FLASH_SECTOR_ADDRESS;

    // Nếu dữ liệu hợp lệ (ví dụ không phải 0xFFFFFFFF)
    if (value != 0xFFFFFFFF)
    {
        highScore = value;
    }
    else
    {
        highScore = 0;
    }
}

void Screen2View::saveGameToFlash()
{
    HAL_FLASH_Unlock();

    // Xóa sector trước khi ghi trạng thái game
    FLASH_EraseInitTypeDef erase;
    uint32_t sectorError;
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector       = FLASH_SECTOR_SAVE; // Giả sử cùng sector hoặc một sector khác nếu cần
    erase.NbSectors    = 1;
    // Cần đảm bảo rằng việc xóa này không ảnh hưởng đến HighScore nếu chúng ở cùng sector
    if (HAL_FLASHEx_Erase(&erase, &sectorError) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return;
    }

    GameState gameState;
    memcpy(gameState.tickCount, tickCount, sizeof(tickCount));
    gameState.score = score;
    gameState.isGamePaused = isGamePaused;

    // Ghi từng phần của GameState vào Flash
    uint32_t address = FLASH_GAME_STATE_ADDRESS;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, gameState.tickCount[i][j]);
            address += 4;
        }
    }
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, (uint32_t)gameState.score);
    address += 4;
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, (uint32_t)gameState.isGamePaused);

    HAL_FLASH_Lock();
}
void Screen2View::loadGameFromFlash()
{
    uint32_t address = FLASH_GAME_STATE_ADDRESS;
    GameState loadedGameState;
    // Đọc từng phần của GameState từ Flash
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            loadedGameState.tickCount[i][j] = *(uint32_t*)address;
            address += 4;
        }
    }
    loadedGameState.score = (int)*(uint32_t*)address;
    address += 4;
    loadedGameState.isGamePaused = (bool)*(uint32_t*)address;

    // Kiểm tra dữ liệu
    bool isValid = false;
    for(int i = 0; i < 4; ++i) {
        for(int j = 0; j < 4; ++j) {
            if(loadedGameState.tickCount[i][j] != 0xFFFFFFFF) {
                isValid = true;
                break;
            }
        }
        if(isValid) break;
    }

    if (isValid) { // Nếu dữ liệu hợp lệ
        memcpy(tickCount, loadedGameState.tickCount, sizeof(tickCount));
        score = loadedGameState.score;
        isGamePaused = loadedGameState.isGamePaused;
    } else {
        // Nếu không hợp lệ => game mới
        initGame();
    }
    updateUI(); // Cập nhật UI sau khi load
}
void Screen2View::setupScreen()
{
    Screen2ViewBase::setupScreen();
    loadHighScoreFromFlash();

    buttonPause.setClickedCallback(buttonCallback);
    buttonResume.setClickedCallback(buttonCallback);
    buttonSave.setClickedCallback(buttonCallback);
    buttonLoad.setClickedCallback(buttonCallback);

    // Đảm bảo ẩn popup ban đầu
    pauseMenuContainer.setVisible(false);

    initGame();
}

void Screen2View::tearDownScreen()
{
    Screen2ViewBase::tearDownScreen();
}

void Screen2View::initGame()
{
    memset(tickCount, 0, sizeof(tickCount));
    score = 0;

    gameOverText.setVisible(false); // Ẩn thông báo thua khi bắt đầu
    gameOverText.invalidate();

    spawnTile();
    HAL_Delay(8);
    spawnTile();
    updateUI();
}

void Screen2View::spawnTile()
{
    int emptyCount = 0;
    int emptyPos[16][2];

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (tickCount[i][j] == 0)
            {
                emptyPos[emptyCount][0] = i;
                emptyPos[emptyCount][1] = j;
                emptyCount++;
            }
        }
    }

    if (emptyCount > 0)
    {
        // Gọi HAL_GetTick() một lần duy nhất
        uint32_t tick = HAL_GetTick() ;

        // Sử dụng tick để sinh số "ngẫu nhiên"
        int idx = tick % emptyCount;
        int i = emptyPos[idx][0];
        int j = emptyPos[idx][1];

        tick = tick ^ (i * 31 + j * 17);

        // Dùng tick tiếp tục để quyết định giá trị 2 hoặc 4
        tickCount[i][j] = ((tick / 10) % 10 == 0) ? 4 : 2;
    }
}

void Screen2View::updateUI()
{
    Unicode::snprintf(scoreBuffer, 10, "%d", score);
    scoreText.setWildcard(scoreBuffer);
    scoreText.invalidate();

    Unicode::snprintf(highScoreBuffer, 10, "%d", highScore);
    highscoreText.setWildcard(highScoreBuffer);
    highscoreText.invalidate();

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (tickCount[i][j] == 0)
                Unicode::snprintf(numBuffer[i][j], 6, "");
            else
                Unicode::snprintf(numBuffer[i][j], 6, "%d", tickCount[i][j]);

            colortype color = getColorForValue(tickCount[i][j]);

            switch (i * 4 + j)
            {
            case 0:  num00.setWildcard(numBuffer[i][j]); num00.invalidate();
                     box00.setColor(color); box00.invalidate(); break;
            case 1:  num01.setWildcard(numBuffer[i][j]); num01.invalidate();
                     box01.setColor(color); box01.invalidate(); break;
            case 2:  num02.setWildcard(numBuffer[i][j]); num02.invalidate();
                     box02.setColor(color); box02.invalidate(); break;
            case 3:  num03.setWildcard(numBuffer[i][j]); num03.invalidate();
                     box03.setColor(color); box03.invalidate(); break;
            case 4:  num10.setWildcard(numBuffer[i][j]); num10.invalidate();
                     box10.setColor(color); box10.invalidate(); break;
            case 5:  num11.setWildcard(numBuffer[i][j]); num11.invalidate();
                     box11.setColor(color); box11.invalidate(); break;
            case 6:  num12.setWildcard(numBuffer[i][j]); num12.invalidate();
                     box12.setColor(color); box12.invalidate(); break;
            case 7:  num13.setWildcard(numBuffer[i][j]); num13.invalidate();
                     box13.setColor(color); box13.invalidate(); break;
            case 8:  num20.setWildcard(numBuffer[i][j]); num20.invalidate();
                     box20.setColor(color); box20.invalidate(); break;
            case 9:  num21.setWildcard(numBuffer[i][j]); num21.invalidate();
                     box21.setColor(color); box21.invalidate(); break;
            case 10: num22.setWildcard(numBuffer[i][j]); num22.invalidate();
                     box22.setColor(color); box22.invalidate(); break;
            case 11: num23.setWildcard(numBuffer[i][j]); num23.invalidate();
                     box23.setColor(color); box23.invalidate(); break;
            case 12: num30.setWildcard(numBuffer[i][j]); num30.invalidate();
                     box30.setColor(color); box30.invalidate(); break;
            case 13: num31.setWildcard(numBuffer[i][j]); num31.invalidate();
                     box31.setColor(color); box31.invalidate(); break;
            case 14: num32.setWildcard(numBuffer[i][j]); num32.invalidate();
                     box32.setColor(color); box32.invalidate(); break;
            case 15: num33.setWildcard(numBuffer[i][j]); num33.invalidate();
                     box33.setColor(color); box33.invalidate(); break;
            }
        }
    }
}


void Screen2View::moveUp()
{
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 3; x++) {
            for (int k = x + 1; k < 4; k++) {
                if (tickCount[k][y] > 0) {
                    if (tickCount[x][y] == 0) {
                        tickCount[x][y] = tickCount[k][y];
                        tickCount[k][y] = 0;
                        x--;
                    } else if (tickCount[x][y] == tickCount[k][y]) {
                        tickCount[x][y] *= 2;
                        tickCount[k][y] = 0;
                        score += tickCount[x][y];
                        if (score > highScore) highScore = score;
                    }
                    break;
                }
            }
        }
    }
}

void Screen2View::moveDown()
{
    for (int y = 0; y < 4; y++) {
        for (int x = 3; x > 0; x--) {
            for (int k = x - 1; k >= 0; k--) {
                if (tickCount[k][y] > 0) {
                    if (tickCount[x][y] == 0) {
                        tickCount[x][y] = tickCount[k][y];
                        tickCount[k][y] = 0;
                        x++;
                    } else if (tickCount[x][y] == tickCount[k][y]) {
                        tickCount[x][y] *= 2;
                        tickCount[k][y] = 0;
                        score += tickCount[x][y];
                        if (score > highScore) highScore = score;
                    }
                    break;
                }
            }
        }
    }
}

void Screen2View::moveLeft()
{
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 3; y++) {
            for (int k = y + 1; k < 4; k++) {
                if (tickCount[x][k] > 0) {
                    if (tickCount[x][y] == 0) {
                        tickCount[x][y] = tickCount[x][k];
                        tickCount[x][k] = 0;
                        y--;
                    } else if (tickCount[x][y] == tickCount[x][k]) {
                        tickCount[x][y] *= 2;
                        tickCount[x][k] = 0;
                        score += tickCount[x][y];
                        if (score > highScore) highScore = score;
                    }
                    break;
                }
            }
        }
    }
}

void Screen2View::moveRight()
{
    for (int x = 0; x < 4; x++) {
        for (int y = 3; y > 0; y--) {
            for (int k = y - 1; k >= 0; k--) {
                if (tickCount[x][k] > 0) {
                    if (tickCount[x][y] == 0) {
                        tickCount[x][y] = tickCount[x][k];
                        tickCount[x][k] = 0;
                        y++;
                    } else if (tickCount[x][y] == tickCount[x][k]) {
                        tickCount[x][y] *= 2;
                        tickCount[x][k] = 0;
                        score += tickCount[x][y];
                        if (score > highScore) highScore = score;
                    }
                    break;
                }
            }
        }
    }
}

void Screen2View::restart()
{
    if (score > highScore)
        highScore = score;

    saveHighScoreToFlash();
    initGame();
}

void Screen2View::tickEvent()
{
	if (isGamePaused) { // Nếu game đang tạm dừng, không xử lý input
    return;
	}
    uint8_t res;

    if (osMessageQueueGetCount(myQueue01Handle) > 0)
    {
        osMessageQueueGet(myQueue01Handle, &res, NULL, osWaitForever);

        switch (res)
        {
        case 'U': moveUp(); break;
        case 'D': moveDown(); break;
        case 'L': moveLeft(); break;
        case 'R': moveRight(); break;
        }

        spawnTile();
        updateUI();

        // Kiểm tra thua
        if (isGameOver()) {
            gameOverText.setVisible(true);
            gameOverText.invalidate();
        }
    }
}

bool Screen2View::isGameOver()
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            if (tickCount[i][j] == 0)
                return false;

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            if (i < 3 && tickCount[i][j] == tickCount[i + 1][j]) return false;
            if (j < 3 && tickCount[i][j] == tickCount[i][j + 1]) return false;
        }
    saveHighScoreToFlash();
    return true;
}
void Screen2View::buttonCallbackHandler(const touchgfx::AbstractButton& src)
{
    if (&src == &buttonPause) {
        handlePauseButtonPressed();
    } else if (&src == &buttonResume) {
        handleResumeButtonPressed();
    } else if (&src == &buttonSave) {
        handleSaveGameButtonPressed();
    } else if (&src == &buttonLoad) {
        handleLoadButtonPressed();
    }
}

void Screen2View::handlePauseButtonPressed()
{
    // 1. Hiển thị popup tạm dừng
    pauseMenuContainer.setVisible(true);
    pauseMenuContainer.invalidate(); // Yêu cầu TouchGFX vẽ lại vùng popup

    // 2. Tạm dừng game logic
    isGamePaused = true;
}

void Screen2View::handleResumeButtonPressed()
{
    // 1. Ẩn popup tạm dừng
    pauseMenuContainer.setVisible(false);
    pauseMenuContainer.invalidate();

    // 2. Tiếp tục game logic
    isGamePaused = false;
}

void Screen2View::handleSaveGameButtonPressed()
{
    saveGameToFlash();
    // Có thể hiển thị một thông báo "Game Saved!"
}

void Screen2View::handleLoadButtonPressed()
{
    loadGameFromFlash();
    isGamePaused = true //pause game
    handleResumeButtonPressed(); // Đảm bảo ẩn menu tạm dừng sau khi load
}
