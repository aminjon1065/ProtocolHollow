# CC0-ассеты в игре (Art-01, [ADR-0043](../ADR/ADR-0043-art-kit-fab-staging-cc0.md))

Источник — Poly Haven, лицензия CC0 1.0 Universal (общественное достояние, указывать авторство не требуется; мы указываем из уважения).
Набор, адреса и SHA-256 файлов — `Build/Art/cc0_textures.json`. Загрузка — `Build/Scripts/fetch-cc0-textures.sh`, импорт — `Build/Python/create_art_kit.py` (1K, `MaxTextureSize` 1024).

| Текстуры в игре | Poly Haven | Где |
|---|---|---|
| `T_MetalPlate_*` | [metal_plate](https://polyhaven.com/a/metal_plate) | полы Доков и хаба (`MI_Hull_Floor`) |
| `T_RustyMetal_*` | [rusty_metal_02](https://polyhaven.com/a/rusty_metal_02) | стены Доков и хаба (`MI_Hull_Wall`) |
| `T_CorrugatedIron_*` | [corrugated_iron_03](https://polyhaven.com/a/corrugated_iron_03) | потолки Доков и хаба (`MI_Hull_Ceiling`) |
| `T_CathedralStone_*` | [castle_wall_slates](https://polyhaven.com/a/castle_wall_slates) | стены и своды Собора |
| `T_CathedralMarble_*` | [marble_01](https://polyhaven.com/a/marble_01) | полы Собора |
| `T_CathedralPier_*` | [plastered_stone_wall](https://polyhaven.com/a/plastered_stone_wall) | опоры Собора |

Меши кита (`/Game/Hollow/Art/Kit`) — собственные, их строит `Build/Blender/hollow_kit.py`; сторонних моделей в игре нет.
