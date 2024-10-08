#include "UI.h"

UI::UI(string name, Color color, bool newLedLayer) {
  this->name = name;
  this->nameColor = color;
  this->newLedLayer = newLedLayer;
}

// TODO, make new led layer
void UI::Start() {
  status = 0;
  if (newLedLayer)
  {
    prev_layer = MatrixOS::LED::CurrentLayer();
    current_layer = MatrixOS::LED::CreateLayer();
  }
  else
  {
    prev_layer = 0;
    current_layer = MatrixOS::LED::CurrentLayer();
  }
  
  MatrixOS::KEYPAD::Clear();
  Setup();
  while (status != -1)
  {
    LoopTask();
    Loop();
    RenderUI();
  }
  End();
  UIEnd();
}

void UI::Exit() {
  MLOGD("UI", "UI Exit signaled");
  status = -1;
}

void UI::LoopTask() {
  GetKey();
}

void UI::RenderUI() {
  if (uiTimer.Tick(uiUpdateMS) || needRender)
  {
    needRender = false;
    MatrixOS::LED::Fill(0);
    PreRender();
    for (auto const& uiComponentPair : uiComponents)
    {
      if (uiComponentPair.second->IsEnabled() == false) { continue; }
      Point xy = uiComponentPair.first;
      UIComponent* uiComponent = uiComponentPair.second;
      uiComponent->Render(xy);
    }
    PostRender();
    if(crossfade_start_time != UINT32_MAX && newLedLayer)
    {
      RenderCrossfade(prev_layer, current_layer, crossfade_start_time);
    }
    else
    {
      MatrixOS::LED::Update();
    }
  }
}

void UI::GetKey() {
  struct KeyEvent keyEvent;
  while (MatrixOS::KEYPAD::Get(&keyEvent))
  {
    // MLOGD("UI", "Key Event %d %d", keyEvent.id, keyEvent.info.state);
    if (!CustomKeyEvent(&keyEvent)) //Run Custom Key Event first. Check if UI event is blocked
      UIKeyEvent(&keyEvent);
    else
      MLOGD("UI", "KeyEvent Skip: %d", keyEvent.id);
  }
}

void UI::UIKeyEvent(KeyEvent* keyEvent) {
  // MLOGD("UI Key Event", "%d - %d", keyID, keyInfo->state);
  if (keyEvent->id == FUNCTION_KEY)
  {
    if (!disableExit && keyEvent->info.state == RELEASED)
    {
      MLOGD("UI", "Function Key Exit");
      Exit();
      return;
    }
  }
  Point xy = MatrixOS::KEYPAD::ID2XY(keyEvent->id);
  if (xy)
  {
    // MLOGD("UI", "UI Key Event X:%d Y:%d", xy.x, xy.y);
    bool hasAction = false;
    for (auto const& uiComponentPair : uiComponents)
    {
      if (uiComponentPair.second->IsEnabled() == false) { continue; }
      Point relative_xy = xy - uiComponentPair.first;
      UIComponent* uiComponent = uiComponentPair.second;
      if (uiComponent->GetSize().Contains(relative_xy))  // Key Found
      { hasAction |= uiComponent->KeyEvent(relative_xy, &keyEvent->info); }
    }
    // if(hasAction)
    // { needRender = true; }
    if (this->name.empty() == false && hasAction == false && keyEvent->info.state == HOLD && Dimension(Device::x_size, Device::y_size).Contains(xy))
    { MatrixOS::UIInterface::TextScroll(this->name, this->nameColor); }
  }
}

void UI::AddUIComponent(UIComponent* uiComponent, Point xy) {
  // ESP_LOGI("Add UI Component", "%d %d %s", xy.x, xy.y, uiComponent->GetName().c_str());
  uiComponents.push_back(pair<Point, UIComponent*>(xy, uiComponent));
}

void UI::AddUIComponent(UIComponent* uiComponent, uint16_t count, ...) {
  va_list valst;
  va_start(valst, count);
  for (uint8_t i = 0; i < count; i++)
  {
    uiComponents.push_back(pair<Point, UIComponent*>((Point)va_arg(valst, Point), uiComponent));
  }
}

void UI::AllowExit(bool allow) {
  disableExit = !allow;
}

void UI::SetSetupFunc(std::function<void()> setup_func) {
  UI::setup_func = &setup_func;
}

void UI::SetLoopFunc(std::function<void()> loop_func) {
  UI::loop_func = &loop_func;
}

void UI::SetEndFunc(std::function<void()> end_func) {
  UI::end_func = &end_func;
}

void UI::SetPreRenderFunc(std::function<void()> pre_render_func) {
  UI::pre_render_func = &pre_render_func;
}

void UI::SetPostRenderFunc(std::function<void()> post_render_func) {
  UI::post_render_func = &post_render_func;
}

void UI::SetKeyEventHandler(std::function<bool(KeyEvent*)> key_event_handler){
  UI::key_event_handler = &key_event_handler;
}

void UI::ClearUIComponents() {
  uiComponents.clear();
}

void UI::UIEnd() {
  MLOGD("UI", "UI Exited");
  crossfade_start_time = 0;

  MatrixOS::KEYPAD::Clear();


  if (newLedLayer)
  { 
    while(crossfade_start_time != UINT32_MAX)
  {
    RenderCrossfade(current_layer, prev_layer, crossfade_start_time);
    Loop();
  }
  MatrixOS::LED::DestoryLayer(); }
  else
  { MatrixOS::LED::Fill(0); }

  // MatrixOS::LED::Update();
}

void UI::SetFPS(uint16_t fps)
{
  if (fps == 0)
    uiUpdateMS = UINT32_MAX;
  else
    uiUpdateMS = 1000 / fps;
}

void UI::RenderCrossfade(int8_t crossfade_source, int8_t crossfade_target, uint32_t& crossfade_start_time, uint16_t crossfade_duration)
{
  if(crossfade_start_time == 0)
  {
    crossfade_start_time = MatrixOS::SYS::Millis();
  }
  
  if (crossfade_source == -1 || crossfade_target == -1)
  {
    crossfade_start_time = UINT32_MAX;
  }

  uint32_t crossfade_completion = (MatrixOS::SYS::Millis() - crossfade_start_time) * FRACT16_MAX / crossfade_duration;

  if (crossfade_completion >= FRACT16_MAX)
  {
    crossfade_start_time = UINT32_MAX;
    crossfade_completion = FRACT16_MAX;
  }

  MLOGD("UI", "Crossfade %d %d %d - (%d => %d => %d)", crossfade_source, crossfade_target, crossfade_completion, crossfade_start_time, MatrixOS::SYS::Millis(), (crossfade_start_time + crossfade_duration));

  MatrixOS::LED::Crossfade(crossfade_source, crossfade_target, (Fract16)crossfade_completion);
}

void UI::Crossfade(int8_t crossfade_source, int8_t crossfade_target, uint16_t crossfade_duration)
{
  uint32_t crossfade_start_time = MatrixOS::SYS::Millis();
  Timer FadeOutTimer;
  while(crossfade_start_time != UINT32_MAX)
  {
    if (FadeOutTimer.Tick(1000 / UI_DEFAULT_FPS))
    {
      RenderCrossfade(crossfade_source, crossfade_target, crossfade_start_time, crossfade_duration);
    }
  }
}

void UI::FadeOut(uint16_t crossfade_duration)
{
  uint8_t current_layer = MatrixOS::LED::CurrentLayer();
  uint32_t crossfade_start_time = MatrixOS::SYS::Millis();
  Timer FadeOutTimer;
  while(crossfade_start_time != UINT32_MAX)
  {
    if (FadeOutTimer.Tick(1000 / UI_DEFAULT_FPS))
    {
      RenderCrossfade(current_layer, 0, crossfade_start_time, crossfade_duration);
    }
  }
}