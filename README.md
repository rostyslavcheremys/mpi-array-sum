# mpi-array-sum

Сума елементів масиву за допомогою MPI (OpenMPI) на кластері з віртуальних машин Multipass. Мова програм: C. Розгортання і запуск кластера автоматизовано скриптами на Python.

| Завдання | MPI-версія | Оригінал на C# |
|---|---|---|
| 1. Сума масиву | [`src/task_1.c`](src/task_1.c) | [multithreaded-array-sum-csharp](https://github.com/rostyslavcheremys/multithreaded-array-sum-csharp) |
| 2. «Хвильова» сума масиву | [`src/task_2.c`](src/task_2.c) | [wave-array-sum](https://github.com/rostyslavcheremys/wave-array-sum) |

## Структура репозиторію

```
mpi-array-sum/
├── cluster/
│   ├── cloud-init.yaml   # пакети, що ставляться у кожну ВМ (OpenMPI, NFS, gcc)
│   ├── set_up_mpi.py     # створення і налаштування кластера
│   └── run_mpi.py        # компіляція і запуск програм на кластері
├── src/
│   ├── task_1.c          # завдання 1
│   └── task_2.c          # завдання 2
└── README.md
```

## Алгоритми

**Завдання 1: `task_1.c`.** Процес 0 створює масив із одиниць і рахує суму послідовно (для порівняння). Далі:
1. `MPI_Scatterv` розподіляє масив блоками між усіма процесами. Блоки різного розміру, якщо `N` не ділиться націло на кількість процесів.
2. Кожен процес рахує суму свого блоку.
3. `MPI_Reduce` з операцією `MPI_SUM` збирає часткові суми на процесі 0.

**Завдання 2: `task_2.c`.** Алгоритм як у `WaveMethod.cs`: на кроці довжини `L` елемент `a[i]` додається до `a[L-1-i]` для `i < L/2`, потім `L = (L+1)/2`, доки `L > 1`. Результат лежить в `a[0]`.

Масив розподілений блоками між процесами, тому пара `a[i]` і `a[L-1-i]` може опинитися в різних процесах. На кожному кроці:
1. Власники елементів верхньої половини надсилають їх власникам пар через `MPI_Alltoallv`.
2. Власники нижньої половини додають отримане до своїх елементів.

## Вимоги

- [Multipass](https://canonical.com/multipass/install) (віртуальні машини Ubuntu)
- Python 3
- Приблизно 8 ГБ вільної RAM для 2 ВМ по 2 ГБ і вільне місце на диску (по 10 ГБ на ВМ)

OpenMPI, NFS і `gcc` ставити на хост-систему не потрібно: усе встановлюється всередині ВМ автоматично.

### Встановлення залежностей

#### macOS

```bash
brew install multipass
brew install python    # якщо python3 ще не встановлено
```

Перевірка:
```bash
multipass version
python3 --version
```
У командах нижче використовуйте `python3`.

#### Windows

1. Встановіть Multipass з [офіційного сайту](https://canonical.com/multipass/install). Під час встановлення оберіть гіпервізор:
    - **Hyper-V** для Windows Pro, Enterprise або Education (компонент Hyper-V має бути ввімкнений);
    - **VirtualBox** для Windows Home. Спочатку встановіть VirtualBox, а потім виконайте `multipass set local.driver=virtualbox`.
2. Встановіть Python 3 з [python.org](https://www.python.org/downloads/). У майстрі встановлення відмітьте **Add python.exe to PATH**.
3. Відкрийте **новий** PowerShell (щоб підхопився оновлений `PATH`) і перевірте:
   ```powershell
   multipass version
   python --version
   ```

У командах нижче замість `python3` використовуйте `python` (або `py`). Шляхи можна писати як із `/`, так і з `\`, наприклад `..\src\task_1.c`.

#### Linux

```bash
sudo snap install multipass
sudo apt install python3
```

## Швидкий старт

Усі команди виконуються з папки `cluster/` (macOS/Linux: термінал, Windows: PowerShell). Нижче використано `python3`, на Windows замініть його на `python`.

### 1. Розгорнути кластер

```bash
cd cluster
python3 set_up_mpi.py 2 --cpu 2 --ram 2048
```

Параметри: перше число це кількість ВМ (одна `manager` і решта `worker1…N`), `--cpu` ядра на ВМ, `--ram` пам'ять у МБ. Створення займає кілька хвилин: кожна ВМ завантажує образ Ubuntu і пакети. Скрипт нічого не питає і не потрібно його переривати.

Що відбувається під час розгортання:
1. Створюються ВМ із `cluster/cloud-init.yaml` (OpenMPI, NFS, `build-essential`).
2. Кожна нода отримує IP-адреси інших у `/etc/hosts`.
3. Генеруються SSH-ключі і налаштовується вхід між нодами без пароля.
4. `manager` стає NFS-сервером, папка `~/cloud` монтується на всіх `worker`.
5. У `~/cloud/mpi_hosts` записується список вузлів для `mpiexec`.

### 2. Перевірити кластер

```bash
multipass list
multipass exec manager -- mpicc --version
multipass exec manager -- cat /home/ubuntu/cloud/mpi_hosts
multipass exec worker1 -- df -h /home/ubuntu/cloud
multipass exec manager -- ssh worker1 hostname
```

Очікується: усі ВМ у стані `Running`, версія `gcc`, імена нод у `mpi_hosts`, NFS-монтування `manager:/home/ubuntu/cloud` та відповідь `worker1` без запиту пароля.

### 3. Скомпілювати й запустити

Спочатку компіляція, потім запуск (це окремі команди):

```bash
python3 run_mpi.py --c ../src/task_1.c
python3 run_mpi.py --r task_1 -n 4

python3 run_mpi.py --c ../src/task_2.c
python3 run_mpi.py --r task_2 -n 4
```

- `--c <файл.c>` копіює файл на `manager` у `~/cloud` і компілює його через `mpicc -O2`.
- `--r <ім'я>` запускає скомпільовану програму (ім'я файлу без `.c`) через `mpiexec -hostfile ~/cloud/mpi_hosts`.
- `-n N` задає кількість процесів. Без нього їх буде стільки, скільки ядер на ВМ.

Після зміни `.c`-файлу його потрібно скомпілювати заново командою `--c`.

### Власний розмір масиву

Розмір масиву за замовчуванням 100 000 000. Щоб задати інший, запустіть `mpiexec` напряму на `manager`:

```bash
multipass exec manager -- mpiexec -hostfile /home/ubuntu/cloud/mpi_hosts -n 4 /home/ubuntu/cloud/task_1 20000000
```

## Приклад виводу

Завдання 1:
```
Array size: <N>
Processes: <P>

Sequential:
  Sum  = <сума>
  Time = <мс> ms

MPI:
  Sum  = <сума>
  Time = <мс> ms
  Check = OK
```

Завдання 2:
```
Array size: <N>
Processes: <P>

MPI:
  Sum  = <сума>
  Time = <мс> ms
  Check = OK
```

`Check = OK` означає, що отримана сума збігається з очікуваною.

## Обмеження та поради

- **Пам'ять.** У завданні 1 процес 0 тримає весь масив (`N × 4` байти), у завданні 2 кожен процес зберігає свою частину в `long long` (`N × 8` байт загалом). Для `N = 10⁹` на ВМ із 2 ГБ пам'яті не вистачить, тому використовуйте `N` порядку `10⁸` або менше.
- **Кількість процесів.** `mpi_hosts` містить лише імена нод, тому `mpiexec` бере кількість слотів за кількістю ядер. Якщо `-n` більше, ніж є ядер, `mpiexec` відмовиться запускатися. Додайте `--oversubscribe` у рядок `mpi_command` у `cluster/run_mpi.py`.
- **Швидкодія.** Усі ВМ працюють на одному комп'ютері, а масив передається процесам через `MPI_Scatterv`. Сама сума проста, тому час на пересилання даних може перевищити виграш від паралелізму, і прискорення буде менше одиниці. Для задач із такою малою кількістю обчислень на елемент це очікуваний результат.

## Типові проблеми

| Помилка | Причина і розв'язання |
|---|---|
| `cannot connect to the multipass socket` | Не запущений демон Multipass. **macOS:** `sudo launchctl bootstrap system /Library/LaunchDaemons/com.canonical.multipassd.plist`. **Windows:** перезапустіть службу Multipass (`services.msc`) або перезавантажте ПК. **Linux:** `sudo snap restart multipass` |
| `You don't have enough free space in /var/cache/apt/archives/` | Малий диск ВМ. У `cluster/set_up_mpi.py` додано `--disk 10G`, а в `cluster/cloud-init.yaml` немає повного оновлення системи |
| `mpicc: command not found` | `cloud-init` ще не встановив пакети або завершився з помилкою. Перевірте `multipass exec manager -- cloud-init status --long` |
| `command not found: python` або `'python' is not recognized` | На macOS і Linux використовуйте `python3`, на Windows `python` або `py` |
| `There are not enough slots available` | Процесів більше, ніж ядер. Зменште `-n` або додайте `--oversubscribe` |
