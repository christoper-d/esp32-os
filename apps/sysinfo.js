// =========================================================
// apps/sysinfo.js - Pruebas de Hardware, Flash y Botones
// (Movido desde g4briel para dejar g4briel con GameBoy)
// =========================================================
const sysApp = {
  title: "SISTEMA & TESTS",
  items: [
    { id: "test_rgb", title: "Test Pantalla RGB", icon: "🎨", sub: "Probar píxeles e IPS" },
    { id: "test_btn", title: "Test 3 Botones",    icon: "🕹️", sub: "Probar Pines 25, 26, 27" },
    { id: "test_mem", title: "Memoria & Scripts", icon: "💻", sub: "LittleFS, RAM y GitHub" },
    { id: "back",     title: "◀ Volver al Menú",  icon: "↩️", sub: "Regresar al inicio" }
  ]
};
