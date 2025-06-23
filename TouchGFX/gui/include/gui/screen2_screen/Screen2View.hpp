#ifndef SCREEN2VIEW_HPP
#define SCREEN2VIEW_HPP

#include <gui_generated/screen2_screen/Screen2ViewBase.hpp>
#include <gui/screen2_screen/Screen2Presenter.hpp>

#include <touchgfx/widgets/Button.hpp>
#include <touchgfx/widgets/Box.hpp>
#include <touchgfx/widgets/TextArea.hpp>
#include <touchgfx/containers/Container.hpp>

class Screen2View : public Screen2ViewBase
{
public:
    Screen2View();
    virtual ~Screen2View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void tickEvent();
    virtual void restart();
    // Các hàm xử lý sự kiện nút UI
    void handlePauseButtonPressed();
    void handleResumeButtonPressed();
    void handleSaveGameButtonPressed();
    void handleLoadButtonPressed();
protected:
    uint32_t tickCount[4][4] = {{0}};
    int score = 0;

    // Biến cờ để kiểm soát trạng thái tạm dừng game
    bool isGamePaused;

    Unicode::UnicodeChar numBuffer[4][4][6];
    Unicode::UnicodeChar scoreBuffer[10];
    Unicode::UnicodeChar highScoreBuffer[10];

    void initGame();
    void spawnTile();
    void updateUI();

    void moveUp();
    void moveDown();
    void moveLeft();
    void moveRight();
    bool isGameOver();
    void saveHighScoreToFlash();
    void loadHighScoreFromFlash();

    // Thêm cấu trúc để lưu trạng thái game
    struct GameState {
        uint32_t tickCount[4][4];
        int score;
        bool isGamePaused;
    };

    // Callback handler
    touchgfx::Callback<Screen2View, const touchgfx::AbstractButton&> buttonCallback;
    void buttonCallbackHandler(const touchgfx::AbstractButton& src);
};

#endif // SCREEN2VIEW_HPP
