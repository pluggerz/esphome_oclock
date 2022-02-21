#pragma once

class SlaveSettings {
    // 0..8: 
    int background_mode_ = 0;
    // 0..2: 
    int foreground_mode_ = 0;
    // 0..5: is managed by the class LedAsync
    int scaled_brightness_ = 5;
    
    
public:
    int get_scaled_brightness() const {
        return scaled_brightness_;
    }

    void set_scaled_brightness(int value);
    
    int get_background_mode() const {
        return background_mode_;
    }
    void set_background_mode(int value);
    
    int get_foreground_mode() const {
        return foreground_mode_;
    }
    void set_foreground_mode(int value);
} extern slave_settings;