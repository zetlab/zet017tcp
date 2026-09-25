# Формат файла devices.cfg

Описание формата конфигурационного файла для устройств производства **ZETLAB**.

## Общее описание

Файл `devices.cfg` — это XML-документ, содержащий конфигурацию устройства и его измерительных каналов. Файл используется для сохранения и загрузки настроек устройства: параметров АЦП, ЦАП, каналов измерения, настроек тензоизмерений и встроенного генератора.

Формат предназначен для программного чтения библиотекой `zet017tcp` (пример `example2_zet017tcp.c`), которая обеспечивает взаимодействие с устройствами ZET 017, ZET 038, ZET 028 и ZET 058 по протоколу TCP/IP.

## Корневой элемент: `<Config>`

Корневой элемент документа.

| Атрибут | Тип | Обязательный | Описание |
|---------|-----|--------------|----------|
| `version` | string | да | Версия формата. Текущая поддерживаемая версия — `"1.2"`. Библиотека проверяет соответствие и возвращает ошибку при несовпадении. |

Пример:
```xml
<Config version="1.2">
    ...
</Config>
```

## Элемент `<Device>`

Описывает одно физическое устройство (тензостанцию).

| Атрибут | Тип | Описание |
|---------|-----|----------|
| `name` | string | Человекочитаемое имя устройства. Пример: `"ZET 058 №712403"`. |
| `type` | int | Код типа устройства. Для ZET 058 — `14`. |
| `serial` | uint32 | Серийный номер устройства. Библиотека выбирает элемент `<Device>`, у которого `serial` совпадает с серийным номером подключённого устройства. |

## Общие параметры устройства (дочерние элементы `<Device>`)

| Элемент | Тип | Описание |
|---------|-----|----------|
| `<Channel>` | uint32 | Битовая маска активных каналов АЦП. |
| `<ChannelDAC>` | uint32 | Маска каналов ЦАП. |
| `<HCPChannel>` | uint32 | Маска ICP каналов АЦП. |
| `<ModaADC>` | uint16 | Режим работы АЦП (0: по умолчанию (25 кГц), 1: 50 кГц, 2: 25 кГц, 3: 5 кГц, 4: 2.5 кГц). |
| `<RateDAC>` | uint32 | Режим работы ЦАП (400: 200 кГц, 800: 100 кГц, 1600: 50 кГц, 3200: 25 кГц). |
| `<KodAmplify>` | csv uint16 | Список кодов коэффициентов усиления (до 32 значений, разделитель — запятая). |

### Встроенный генератор (Built-in Generator)

| Элемент | Тип | Описание |
|---------|-----|----------|
| `<BuiltinGenActive>` | int | Активность встроенного генератора. |
| `<BuiltinGenSineActive>` | int | Активность синусоидального генератора. |
| `<BuiltinGenSineFreq>` | float | Частота синусоидального сигнала (Гц). |
| `<BuiltinGenSineAmpl>` | float | Амплитуда синусоидального сигнала. |
| `<BuiltinGenSineBias>` | float | Смещение (bias) синусоидального сигнала. |

## Элемент `<Channels>`

Контейнер для описания измерительных каналов. Содержит один или несколько элементов `<Channel>`.

### Элемент `<Channel>` (внутри `<Channels>`)

Описывает отдельный измерительный канал (индивидуальную настройку канала).

#### Дочерние элементы `<Channel>`

| Элемент | Тип | Описание |
|---------|-----|----------|
| `<Tenso>` | int | Схема тензоизмерения: 0 - мостовая, 1 - 1/2-мостовая, 2 - 1/4-мостовая. |
| `<Pot1>` | uint8 | Балансировка схемы измерений (0–255). Данное корректирующее значение физически балансирует одно плечо моста. |
| `<Pot2>` | uint8 | Выбор значения добавочного резистора 1/4-мостовой схемы (0–255). Значение выставляется в соответствии с таблицей ниже. |


| Значение | Сопротивление, Ом | Значение | Сопротивление, Ом | Значение | Сопротивление, Ом | Значение | Сопротивление, Ом |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 0 | 449.3 | 64 | 443.0 | 128 | 431.0 | 192 | 399.9 |
| 1 | 449.2 | 65 | 442.8 | 129 | 430.7 | 193 | 399.0 |
| 2 | 449.2 | 66 | 442.7 | 130 | 430.4 | 194 | 398.1 |
| 3 | 449.1 | 67 | 442.6 | 131 | 430.1 | 195 | 397.2 |
| 4 | 449.0 | 68 | 442.4 | 132 | 429.9 | 196 | 396.2 |
| 5 | 448.9 | 69 | 442.3 | 133 | 429.6 | 197 | 395.2 |
| 6 | 448.9 | 70 | 442.2 | 134 | 429.3 | 198 | 394.2 |
| 7 | 448.8 | 71 | 442.0 | 135 | 429.0 | 199 | 393.2 |
| 8 | 448.7 | 72 | 441.9 | 136 | 428.7 | 200 | 392.1 |
| 9 | 448.6 | 73 | 441.7 | 137 | 428.4 | 201 | 391.0 |
| 10 | 448.5 | 74 | 441.6 | 138 | 428.1 | 202 | 389.9 |
| 11 | 448.4 | 75 | 441.5 | 139 | 427.8 | 203 | 388.8 |
| 12 | 448.4 | 76 | 441.3 | 140 | 427.4 | 204 | 387.6 |
| 13 | 448.3 | 77 | 441.2 | 141 | 427.1 | 205 | 386.4 |
| 14 | 448.2 | 78 | 441.0 | 142 | 426.8 | 206 | 385.1 |
| 15 | 448.1 | 79 | 440.9 | 143 | 426.4 | 207 | 383.8 |
| 16 | 448.0 | 80 | 440.7 | 144 | 426.1 | 208 | 382.5 |
| 17 | 447.9 | 81 | 440.6 | 145 | 425.8 | 209 | 381.1 |
| 18 | 447.9 | 82 | 440.4 | 146 | 425.4 | 210 | 379.6 |
| 19 | 447.8 | 83 | 440.2 | 147 | 425.1 | 211 | 378.2 |
| 20 | 447.7 | 84 | 440.1 | 148 | 424.7 | 212 | 376.6 |
| 21 | 447.6 | 85 | 439.9 | 149 | 424.3 | 213 | 375.1 |
| 22 | 447.5 | 86 | 439.8 | 150 | 424.0 | 214 | 373.4 |
| 23 | 447.4 | 87 | 439.6 | 151 | 423.6 | 215 | 371.8 |
| 24 | 447.3 | 88 | 439.4 | 152 | 423.2 | 216 | 370.0 |
| 25 | 447.2 | 89 | 439.3 | 153 | 422.8 | 217 | 368.2 |
| 26 | 447.1 | 90 | 439.1 | 154 | 422.4 | 218 | 366.3 |
| 27 | 447.0 | 91 | 438.9 | 155 | 422.0 | 219 | 364.4 |
| 28 | 446.9 | 92 | 438.8 | 156 | 421.6 | 220 | 362.4 |
| 29 | 446.9 | 93 | 438.6 | 157 | 421.2 | 221 | 360.3 |
| 30 | 446.8 | 94 | 438.4 | 158 | 420.7 | 222 | 358.1 |
| 31 | 446.7 | 95 | 438.2 | 159 | 420.3 | 223 | 355.8 |
| 32 | 446.6 | 96 | 438.1 | 160 | 419.9 | 224 | 353.5 |
| 33 | 446.5 | 97 | 437.9 | 161 | 419.4 | 225 | 351.0 |
| 34 | 446.4 | 98 | 437.7 | 162 | 418.9 | 226 | 348.4 |
| 35 | 446.3 | 99 | 437.5 | 163 | 418.5 | 227 | 345.8 |
| 36 | 446.2 | 100 | 437.3 | 164 | 418.0 | 228 | 343.0 |
| 37 | 446.1 | 101 | 437.1 | 165 | 417.5 | 229 | 340.0 |
| 38 | 446.0 | 102 | 436.9 | 166 | 417.0 | 230 | 337.0 |
| 39 | 445.9 | 103 | 436.7 | 167 | 416.5 | 231 | 333.7 |
| 40 | 445.8 | 104 | 436.5 | 168 | 416.0 | 232 | 330.4 |
| 41 | 445.7 | 105 | 436.3 | 169 | 415.5 | 233 | 326.8 |
| 42 | 445.5 | 106 | 436.1 | 170 | 415.0 | 234 | 323.1 |
| 43 | 445.4 | 107 | 435.9 | 171 | 414.4 | 235 | 319.1 |
| 44 | 445.3 | 108 | 435.7 | 172 | 413.9 | 236 | 315.0 |
| 45 | 445.2 | 109 | 435.5 | 173 | 413.3 | 237 | 310.6 |
| 46 | 445.1 | 110 | 435.3 | 174 | 412.7 | 238 | 306.0 |
| 47 | 445.0 | 111 | 435.1 | 175 | 412.1 | 239 | 301.0 |
| 48 | 444.9 | 112 | 434.9 | 176 | 411.5 | 240 | 295.8 |
| 49 | 444.8 | 113 | 434.6 | 177 | 410.9 | 241 | 290.3 |
| 50 | 444.7 | 114 | 434.4 | 178 | 410.3 | 242 | 284.3 |
| 51 | 444.6 | 115 | 434.2 | 179 | 409.7 | 243 | 278.0 |
| 52 | 444.4 | 116 | 434.0 | 180 | 409.0 | 244 | 271.2 |
| 53 | 444.3 | 117 | 433.7 | 181 | 408.3 | 245 | 264.0 |
| 54 | 444.2 | 118 | 433.5 | 182 | 407.6 | 246 | 256.1 |
| 55 | 444.1 | 119 | 433.3 | 183 | 406.9 | 247 | 247.7 |
| 56 | 444.0 | 120 | 433.0 | 184 | 406.2 | 248 | 238.6 |
| 57 | 443.8 | 121 | 432.8 | 185 | 405.5 | 249 | 228.7 |
| 58 | 443.7 | 122 | 432.5 | 186 | 404.8 | 250 | 217.9 |
| 59 | 443.6 | 123 | 432.3 | 187 | 404.0 | 251 | 206.0 |
| 60 | 443.5 | 124 | 432.0 | 188 | 403.2 | 252 | 193.1 |
| 61 | 443.3 | 125 | 431.8 | 189 | 402.4 | 253 | 178.7 |
| 62 | 443.2 | 126 | 431.5 | 190 | 401.6 | 254 | 162.9 |
| 63 | 443.1 | 127 | 431.2 | 191 | 400.7 | 255 | 145.1 |

## Взаимосвязь с библиотекой `zet017tcp`

Библиотека `zet017tcp` (C API) предоставляет функции для работы с конфигурацией:

- `zet017_device_get_config()` — получить текущую конфигурацию устройства.
- `zet017_device_set_config()` — установить конфигурацию.
- `zet017_device_get_tenso_config()` — получить настройки тензоизмерений.
- `zet017_device_set_tenso_config()` — установить настройки тензоизмерений.

Пример `example2_zet017tcp.c` демонстрирует полный цикл:

1. Получение конфигурации с устройства.
2. Загрузка XML-файла через `load_config()`.
3. Применение конфигурации к устройству.
4. Запуск измерений.

### Соответствие полей XML и структур C

| XML-элемент | Поле в структуре C |
|-------------|-------------------|
| `<Channel>` (общий) | `mask_channel_adc` |
| `<HCPChannel>` | `mask_icp` |
| `<ModaADC>` | `moda_adc` |
| `<RateDAC>` | `rate_dac` |
| `<KodAmplify>` | `gain_code[]` |
| `<BuiltinGenActive>` | `builtin_dac_state` (бит 0) |
| `<BuiltinGenSineActive>` | `builtin_dac_state` (бит 1) |
| `<BuiltinGenSineFreq>` | `builtin_dac_sine_freq` |
| `<BuiltinGenSineAmpl>` | `builtin_dac_sine_ampl` |
| `<BuiltinGenSineBias>` | `builtin_dac_sine_offset` |
| `<Tenso>` (в `<Channel>`) | `tenso_config->scheme[i]` |
| `<Pot1>` (в `<Channel>`) | `tenso_config->correction_1[i]` |
| `<Pot2>` (в `<Channel>`) | `tenso_config->correction_2[i]` |

## Ограничения

- **Версия формата**: поддерживается только `"1.2"`. При несовпадении `load_config()` возвращает ошибку.
- **Серийный номер**: обязателен. Если устройство с указанным `serial` не найдено, загрузка конфигурации завершается ошибкой.
- **Количество каналов**: библиотека обрабатывает каналы с `id < 8`. Каналы с большим `id` игнорируются.
- **Размер массивов**: количество значений в `<KodAmplify>` и `<Amplitude>` ограничено 32.

## Пример полного файла

```xml
<?xml version="1.0"?>
<Config version="1.2">
    <Device name="ZET 058 №712403" type="14" serial="712403" label="" configTime="17.03.2026 13:28:18" configName="">
        <typeADC>0</typeADC>
        <Channel>511</Channel>
        <ChannelDAC>0</ChannelDAC>
        <HCPChannel>0</HCPChannel>
        <ModaADC>1</ModaADC>
        <Rate>160</Rate>
        <RateDAC>800</RateDAC>
        <sizeInterrupt>8064</sizeInterrupt>
        <sizeInterruptDAC>512</sizeInterruptDAC>
        <KodAmplify>2,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0</KodAmplify>
        <Amplitude>98.484466552734375,9.9277667999267578,9.9331626892089844,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0</Amplitude>
        <DigitalResolChanADC>6.89049928e-09,6.91036739e-09,6.88457913e-09,6.9067827e-09,6.89982249e-09,6.90562452e-09,6.89290225e-09,6.91877045e-09,6.58371713e-09,4.86616036e-09,4.86616036e-09,4.86616036e-09,4.86616036e-09,4.86616036e-09,4.86616036e-09,4.86616036e-09,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0</DigitalResolChanADC>
        <PRUS>0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0</PRUS>
        <Atten>32767,0,0,0</Atten>
        <EnaExtFreq>0</EnaExtFreq>
        <EnaExtStart>0</EnaExtStart>
        <ExtOporFreq>0</ExtOporFreq>
        <ChannelDiff>0</ChannelDiff>
        <DigitalInput>255</DigitalInput>
        <DigitalOutput>32</DigitalOutput>
        <DigitalOutEnable>0</DigitalOutEnable>
        <EnaExtFreqDAC>0</EnaExtFreqDAC>
        <EnaExtStartDAC>0</EnaExtStartDAC>
        <ExtFreqDAC>50000</ExtFreqDAC>
        <MasterSync>0</MasterSync>
        <BuiltinGenActive>1</BuiltinGenActive>
        <BuiltinGenSineActive>1</BuiltinGenSineActive>
        <BuiltinGenSineFreq>1000</BuiltinGenSineFreq>
        <BuiltinGenSineAmpl>0</BuiltinGenSineAmpl>
        <BuiltinGenSineBias>1</BuiltinGenSineBias>
        <ChargeChannel>0</ChargeChannel>
        <Freq>50000</Freq>
        <Channels>
            <Channel id="0" name="ZET 058_712403_1" comment="" units="мВ" unitsense="В" enabled="true">
                <Sense>0.0010000000474974513</Sense>
                <CoordX>0</CoordX>
                <CoordY>0</CoordY>
                <CoordZ>0</CoordZ>
                <CoordP>0</CoordP>
                <Amplify>1</Amplify>
                <Preamplifier>0</Preamplifier>
                <Reference>0.0010000000474974513</Reference>
                <Shift>0</Shift>
                <AFCH />
                <HPF>0</HPF>
                <Tenso>0</Tenso>
                <TensoIcp state="0" resistance="100" coeff="2" young_mod="200" />
                <InputResistance>0</InputResistance>
                <AdcInputMode>0</AdcInputMode>
                <Pot1>183</Pot1>
                <Pot2>183</Pot2>
            </Channel>
            <Channel id="1" name="ZET 058_712403_2" comment="" units="мкм/м" unitsense="В" enabled="true">
                <Sense>4.9999999873762135e-07</Sense>
                <CoordX>0</CoordX>
                <CoordY>0</CoordY>
                <CoordZ>0</CoordZ>
                <CoordP>0</CoordP>
                <Amplify>1</Amplify>
                <Preamplifier>0</Preamplifier>
                <Reference>1</Reference>
                <Shift>0</Shift>
                <AFCH />
                <HPF>0</HPF>
                <Tenso>2</Tenso>
                <TensoIcp state="0" resistance="100" coeff="2" young_mod="200" />
                <InputResistance>0</InputResistance>
                <AdcInputMode>0</AdcInputMode>
                <Pot1>125</Pot1>
                <Pot2>251</Pot2>
            </Channel>
            <Channel id="2" name="ZET 058_712403_3" comment="" units="мкм/м" unitsense="В" enabled="true">
                <Sense>4.9999999873762135e-07</Sense>
                <CoordX>0</CoordX>
                <CoordY>0</CoordY>
                <CoordZ>0</CoordZ>
                <CoordP>0</CoordP>
                <Amplify>1</Amplify>
                <Preamplifier>0</Preamplifier>
                <Reference>1</Reference>
                <Shift>0</Shift>
                <AFCH />
                <HPF>0</HPF>
                <Tenso>2</Tenso>
                <TensoIcp state="0" resistance="100" coeff="2" young_mod="200" />
                <InputResistance>0</InputResistance>
                <AdcInputMode>0</AdcInputMode>
                <Pot1>126</Pot1>
                <Pot2>251</Pot2>
            </Channel>
            <Channel id="3" name="ZET 058_712403_4" comment="" units="мВ" unitsense="В" enabled="true">
                <Sense>0.0010000000474974513</Sense>
                <CoordX>0</CoordX>
                <CoordY>0</CoordY>
                <CoordZ>0</CoordZ>
                <CoordP>0</CoordP>
                <Amplify>1</Amplify>
                <Preamplifier>0</Preamplifier>
                <Reference>0.0010000000474974513</Reference>
                <Shift>0</Shift>
                <AFCH />
                <HPF>0</HPF>
                <Tenso>0</Tenso>
                <TensoIcp state="0" resistance="100" coeff="2" young_mod="200" />
                <InputResistance>0</InputResistance>
                <AdcInputMode>0</AdcInputMode>
                <Pot1>127</Pot1>
                <Pot2>127</Pot2>
            </Channel>
            <Channel id="4" name="ZET 058_712403_5" comment="" units="мВ" unitsense="В" enabled="true">
                <Sense>0.0010000000474974513</Sense>
                <CoordX>0</CoordX>
                <CoordY>0</CoordY>
                <CoordZ>0</CoordZ>
                <CoordP>0</CoordP>
                <Amplify>1</Amplify>
                <Preamplifier>0</Preamplifier>
                <Reference>0.0010000000474974513</Reference>
                <Shift>0</Shift>
                <AFCH />
                <HPF>0</HPF>
                <Tenso>0</Tenso>
                <TensoIcp state="0" resistance="100" coeff="2" young_mod="200" />
                <InputResistance>0</InputResistance>
                <AdcInputMode>0</AdcInputMode>
                <Pot1>127</Pot1>
                <Pot2>127</Pot2>
            </Channel>
            <Channel id="5" name="ZET 058_712403_6" comment="" units="мВ" unitsense="В" enabled="true">
                <Sense>0.0010000000474974513</Sense>
                <CoordX>0</CoordX>
                <CoordY>0</CoordY>
                <CoordZ>0</CoordZ>
                <CoordP>0</CoordP>
                <Amplify>1</Amplify>
                <Preamplifier>0</Preamplifier>
                <Reference>0.0010000000474974513</Reference>
                <Shift>0</Shift>
                <AFCH />
                <HPF>0</HPF>
                <Tenso>0</Tenso>
                <TensoIcp state="0" resistance="100" coeff="2" young_mod="200" />
                <InputResistance>0</InputResistance>
                <AdcInputMode>0</AdcInputMode>
                <Pot1>127</Pot1>
                <Pot2>127</Pot2>
            </Channel>
            <Channel id="6" name="ZET 058_712403_7" comment="" units="мВ" unitsense="В" enabled="true">
                <Sense>0.0010000000474974513</Sense>
                <CoordX>0</CoordX>
                <CoordY>0</CoordY>
                <CoordZ>0</CoordZ>
                <CoordP>0</CoordP>
                <Amplify>1</Amplify>
                <Preamplifier>0</Preamplifier>
                <Reference>0.0010000000474974513</Reference>
                <Shift>0</Shift>
                <AFCH />
                <HPF>0</HPF>
                <Tenso>0</Tenso>
                <TensoIcp state="0" resistance="100" coeff="2" young_mod="200" />
                <InputResistance>0</InputResistance>
                <AdcInputMode>0</AdcInputMode>
                <Pot1>127</Pot1>
                <Pot2>127</Pot2>
            </Channel>
            <Channel id="7" name="ZET 058_712403_8" comment="" units="мВ" unitsense="В" enabled="true">
                <Sense>0.0010000000474974513</Sense>
                <CoordX>0</CoordX>
                <CoordY>0</CoordY>
                <CoordZ>0</CoordZ>
                <CoordP>0</CoordP>
                <Amplify>1</Amplify>
                <Preamplifier>0</Preamplifier>
                <Reference>0.0010000000474974513</Reference>
                <Shift>0</Shift>
                <AFCH />
                <HPF>0</HPF>
                <Tenso>0</Tenso>
                <TensoIcp state="0" resistance="100" coeff="2" young_mod="200" />
                <InputResistance>0</InputResistance>
                <AdcInputMode>0</AdcInputMode>
                <Pot1>127</Pot1>
                <Pot2>127</Pot2>
            </Channel>
            <Channel id="8" name="Генератор_712403_1" comment="" units="мВ" unitsense="В" enabled="true">
                <Sense>0.001</Sense>
                <CoordX>0</CoordX>
                <CoordY>0</CoordY>
                <CoordZ>0</CoordZ>
                <CoordP>0</CoordP>
                <Amplify>1</Amplify>
                <Preamplifier>0</Preamplifier>
                <Reference>0.001</Reference>
                <Shift>0</Shift>
                <AFCH />
                <HPF>0</HPF>
                <Tenso>0</Tenso>
                <TensoIcp state="0" resistance="100" coeff="2" young_mod="200" />
                <InputResistance>0</InputResistance>
                <AdcInputMode>0</AdcInputMode>
                <Pot1>0</Pot1>
                <Pot2>0</Pot2>
            </Channel>
        </Channels>
    </Device>
</Config>
```

## Пример минимального файла

```xml
<?xml version="1.0"?>
<Config version="1.2">
    <Device serial="712403">
        <Channel>511</Channel>
        <HCPChannel>0</HCPChannel>
        <ModaADC>1</ModaADC>
        <RateDAC>800</RateDAC>
        <KodAmplify>2,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0</KodAmplify>
        <BuiltinGenActive>1</BuiltinGenActive>
        <BuiltinGenSineActive>1</BuiltinGenSineActive>
        <BuiltinGenSineFreq>1000</BuiltinGenSineFreq>
        <BuiltinGenSineAmpl>0</BuiltinGenSineAmpl>
        <BuiltinGenSineBias>1</BuiltinGenSineBias>
        <Channels>
            <Channel id="0" units="мВ">
                <Sense>0.001</Sense>
                <Tenso>0</Tenso>
                <Pot1>183</Pot1>
                <Pot2>183</Pot2>
            </Channel>
        </Channels>
    </Device>
</Config>
```

## Ссылки

- Библиотека `zet017tcp` на GitHub: <https://github.com/zetlab/zet017tcp>
- Документация на тензометрическую систему ZET 058: <https://zetlab.com/shop/tenzostantsii/zet-058/>