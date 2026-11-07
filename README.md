# ChainsawClient

Клиент для [DDNet](https://ddnet.org) — изменённая версия [BestClient](https://bestclient.fun)
(который сам основан на [TClient](https://github.com/TaterClient/TClient) и DDNet).
Только визуал и удобство: ботов в этой версии нет, обновлялка BestClient выключена.

*A DDNet client: a modified version of BestClient (based on TClient and DDNet), visuals only.*

## Что добавлено

**Меню ChainsawClient** — открывается по **Right Shift** (клавишу можно сменить): все наши функции, у каждой тумблер,
правая кнопка мыши — все настройки, наведи и подержи — описание. Градиенты, картинки за меню, свой шрифт, звуки.

**Огонь и лёд**
- огненное кольцо с бусинами за тее, пылающий молот (золотой топор, появляется из огня, разгорается от ударов)
- ударная волна при приземлении и от молота, иней и осколки льда при заморозке

**Цепь и удары** — горящая цепь крюка (огненный жгут с гарпуном), ожоги, огненная вспышка и мега-удар

**Кагуне** — щупальца гуля вместо молота (ринкаку, какуджа, укаку, костяное), атаки, кровь, щупальца вместо крюка, глаз гуля

**Атмосфера** — светлячки, падающие звёзды, гроза с молниями и громом, ветер с листьями, снег, туман, угли;
физический дождь (брызги, лужи на блоках) и снег (сугробы на блоках)

**Шейдеры** (рендер OpenGL) — цвет и свет, свечение (bloom), виньетка, резкость, зерно плёнки, цветные края

**Оптимизация**
- режим «Максимальная оптимизация» (`bc_perf_mode`): выключает всё тяжёлое разом, при выключении возвращает настройки
- на ноутбуках с двумя видеокартами игра просит мощную
- выключенные эффекты BestClient больше не тратят время каждый кадр

**Прочее** — музыкальный плеер BestClient работает в сборке MinGW (сессии Windows без C++/WinRT), слова песен с запасным источником.

Настройки ChainsawClient хранятся в `settings_ChainsawClient.cfg`, отдельно от BestClient.

## Скачать

Готовая сборка для Windows — в [Releases](../../releases). Распакуй и запусти `DDNet.exe`.

## Сборка из исходников (Windows, MinGW-w64)

Нужны: GCC MinGW-w64, CMake, Ninja, Python 3, Rust (cargo).

```
git clone --recursive https://github.com/shinixxx/ChainsawClient
cd ChainsawClient
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release -DVIDEORECORDER=ON -DDISCORD=ON -DSTEAM=ON -DVULKAN=OFF
ninja -C build game-client
```

`ddnet-libs` — подмодуль из [ddnet/ddnet-libs](https://github.com/ddnet/ddnet-libs).

Двух файлов BestClient в репозитории нет, потому что их нельзя свободно распространять:
`data/BestClient/ActayWide-Bold.otf` (шрифт, все права у Arodora LLC) и `data/BestClient/trollface.png`.
Без них клиент работает: вместо шрифта берётся обычный, у стиля крюка «тролль» нет лица.

## Лицензии

- Код DDNet / Teeworlds — лицензия zlib, см. [license.txt](license.txt). Это изменённая версия, не оригинальный DDNet.
- Код BestClient — MIT, см. [LICENSE](LICENSE) (Copyright (c) 2026 BestProject).
- Код TClient — под лицензией DDNet.
- Изменения ChainsawClient — MIT, как и BestClient.
- Файлы в `data/` — под своими лицензиями (CC-BY-SA 3.0 и др., см. [license.txt](license.txt) и `license.txt` в папках).
  Шрифты в `data/tclient/fonts`: Montserrat и Russo One — SIL OFL 1.1 (файлы лицензий рядом),
  Cabin, Inter, Nunito, Rubik, Google Sans — SIL OFL 1.1, FreeSans — GPL с исключением для шрифтов,
  Minecraft (Pwnage_Block, fontstruct.com) — CC-BY-SA 3.0.
- Звуки в `data/chainsaw/sounds` синтезированы для ChainsawClient.

Оригинальное описание BestClient — [README.BestClient.md](README.BestClient.md).
