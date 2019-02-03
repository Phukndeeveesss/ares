struct Options : Setting<> {
  struct Video : Setting<> { using Setting::Setting;
    Setting<boolean> interframeBlending{this, "interframeBlending", true};
    Setting<boolean> colorEmulation{this, "colorEmulation", true};
  } video{this, "video"};

  Options() : Setting{"options"} {
    //todo: why is ::higan needed? fc/interface/options.hpp works fine with just higan
    video.interframeBlending.onModify([&] {
    //if(Model::SuperGameBoy()) return;
      ::higan::video.setEffect(::higan::Video::Effect::InterframeBlending, video.interframeBlending());
    });
    video.colorEmulation.onModify([&] {
    //if(Model::SuperGameBoy()) return;
      ::higan::video.setPalette();
    });
  }
};

extern Options option;