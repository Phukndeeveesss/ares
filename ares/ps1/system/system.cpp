#include <ps1/ps1.hpp>

namespace ares::PlayStation {

System system;
#include "serialization.cpp"

auto System::run() -> void {
  while(!gpu.refreshed) cpu.main();
  gpu.refreshed = false;
  gpu.refresh();
}

auto System::load(Node::Object& root) -> void {
  if(node) unload();

  information = {};

  node = Node::System::create(interface->name());
  root = node;

  cpu.load(node);
  gpu.load(node);
  spu.load(node);
  disc.load(node);
  controllerPort1.load(node);
  controllerPort2.load(node);
  interrupt.load(node);
  peripheral.load(node);
  dma.load(node);
  timer.load(node);
}

auto System::unload() -> void {
  if(!node) return;
  save();
  cpu.unload();
  gpu.unload();
  spu.unload();
  disc.unload();
  controllerPort1.unload();
  controllerPort2.unload();
  interrupt.unload();
  peripheral.unload();
  dma.unload();
  timer.unload();
  node.reset();
}

auto System::save() -> void {
  if(!node) return;
}

auto System::power(bool reset) -> void {
  for(auto& setting : node->find<Node::Setting>()) setting->setLatch();

  bios.allocate(512_KiB);
  if(auto fp = platform->open(node, "bios.rom", File::Read, File::Required)) {
    bios.load(fp);
  }

  cpu.power(reset);
  gpu.power(reset);
  spu.power(reset);
  disc.power(reset);
  interrupt.power(reset);
  peripheral.power(reset);
  dma.power(reset);
  timer.power(reset);

  information.serializeSize[0] = serializeInit(0);
  information.serializeSize[1] = serializeInit(1);
}

}