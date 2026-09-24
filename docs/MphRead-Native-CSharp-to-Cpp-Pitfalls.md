# C# → C++ 移植の落とし穴（実例集）

`MphRead-Native-CSharp-to-Cpp-Basic-Policy.md` の「観測可能な意味を変えない」を、
実際に破った例から逆引きできるようにまとめたもの。どれも「ビルドは通る」「C# と
1行ずつ突き合わせても同じに見える」のに挙動が変わったもので、原因は C# と C++ の
言語・ランタイムの差にある。

各項目は **症状 → 原因 → 修正 → 見つけ方** の順。修正コミットは develop2 のもの。

機械的に探せるものは `tools/native-audit/` にスキャナを置いてある（末尾参照）。
新しい型のバグを見つけたら、この文書に項目を足し、可能ならスキャナも足すこと。

---

## 1. 引数・オペランドの評価順

**症状**: ドアを抜けて次のエリアを読み込む瞬間にクラッシュ（`reading 0x8`）。

**原因**: `Scene::InsertEntity` の

```cpp
_entityMap.Add(entity->Id, std::move(entity));
```

C# は引数を必ず左から評価する。C++ では順序が未規定で、MinGW の GCC (x64) は
**右から**作る。値渡しの第2引数へ `entity` が先に move され、空になった後で
`entity->Id` を読んでいた（`Id` はオフセット 8）。

**修正** (`c17373d`): 先にローカルへ取り出す。

```cpp
const std::int32_t id = entity->Id;
_entityMap.Add(id, std::move(entity));
```

**注意点**

- 同じ呼び出しの中で、ある変数を `std::move` しつつ別の引数で読まない。
- 乱数 (`Rng::GetRandomInt2`)、ストリーム読み込み (`reader.ReadInt32()`)、
  `Next*` / `Pop*` / `Take*` など副作用のある呼び出しを、**丸括弧**の引数や
  `+ - * /` の両辺に2つ以上並べない。C# と呼ぶ順が逆になり、乱数列やバイト列が
  入れ替わる。
- 順序が保証されるのは `{ }` の初期化子（左から）、`&&` `||` `?:` `,`、
  C++17 以降の `a.f(b)` の `a`（引数より先）、代入の右辺→左辺など。迷ったら
  ローカル変数に分ける。

**見つけ方**: `tools/native-audit/eval_order.py`

---

## 2. ヘルパー関数名とクラス名の衝突（名前探索）

**症状**: アーティファクトを取っても記録されない。入り直すと復活し、ボス部屋への
テレポーターが起動しない。ただし取得で解除された扉のロックは残っている。

**原因**: `ArtifactEntity.cpp` にファイルスコープのヘルパー

```cpp
MphRead::StorySave& StorySave() { return *GameState::StorySave; }
```

があったが、呼び出し側は `namespace MphRead::Entities` の中にある。非修飾名の探索は
`MphRead::Entities` → `MphRead` → グローバルの順に進むので、グローバルのヘルパーより
先に **クラス `MphRead::StorySave`** が見つかる。`StorySave().UpdateFoundArtifact(...)`
は空の一時オブジェクトを作ってそれを更新し、すぐ捨てていた。コンパイルエラーには
ならない。

**修正** (`06eec2c`): クラス名と重ならない名前 `RequireStorySave()` にした。

**注意点**

- ファイルスコープの補助関数に、`MphRead` 以下の型と同じ名前を付けない。
  `C#` のプロパティ名（`StorySave`、`Main` など）をそのまま関数名にすると起きやすい。
- 「書いたのに反映されない」「読むといつも初期値」は、まずこれを疑う。

**見つけ方**: `tools/native-audit/name_shadow.py`。調べた範囲では他に実害のある箇所はない
（`Utility/Compress.cpp` の `Read()` は一致するコンストラクタがないのでクラスには解決できず、無害）。

---

## 3. コンテナ要素への参照（「その場所」を渡してしまう）

**症状**: クレタフィッド戦・ボス後の脱出・チャージ中などでランダムに
`vector::_M_range_check: __n (which is 0) >= this->size() (which is 0)`。
投げ元は `Scene::ProcessEffects`。

**原因**: 部屋を出るたびに呼ばれる `Scene::ClearEffects` が

```cpp
UnlinkEffectElement(_activeElements[i]);   // 引数は const shared_ptr&
```

としていた。関数内の `RemoveFirst(_activeElements, element)` で要素が消えて後ろが
詰まると、参照 `element` は**次の（まだ使用中の）要素**を指す。関数の残りはその要素の
`ParticleDefinitions` などを空にして空き要素プールへ戻した。プールに同じ要素が2回入り、
あとで2つのエフェクトが1つの要素を共有し、片方の解放でもう片方の粒子定義が空になった。
C# はオブジェクト参照を渡すので `List.Remove` の後も同じ要素を指し続ける。

**修正** (`b7e8981`): `UnlinkEffectElement` / `UnlinkBeamEffect` / `UnlinkBomb` の
引数を `shared_ptr` の**値渡し**にした。

**注意点**

- C# の「参照型の引数」は `const std::shared_ptr<T>&` に機械的に置き換えない。受け取った
  関数が、渡元のコンテナを変更しうるなら値渡しにする。
- `auto& x = v[i];` のあと `v.push_back(...)` / `erase` してから `x` を使わない
  （再確保で `x` が宙に浮く）。C# では `List` を伸ばしても要素オブジェクトは動かない。

**見つけ方**: `tools/native-audit/slot_alias.py`

---

## 4. デストラクタを持つ `thread_local`（MinGW）

**症状**: スレッド終了時（ムービー・音楽・ボス部屋ロード後など）に、TLS コールバックの中の
`~shared_ptr<...>()` で `0xC0000005`。読んだアドレスが `0x74696E6967`（ASCII の
"ginit"）など、他人のデータ。

**原因**: MinGW の `thread_local` はエミュレーション（emutls）で、スレッド終了時に
格納領域の解放が、そこに置かれたオブジェクトのデストラクタより先に走ることがある。
C# の `[ThreadStatic]` にはこの問題がない。

**修正** (`80a79bf`): デストラクタを持つ `thread_local` をなくした。

- `NativeRuntime/System/ThreadStatic.hpp` の `ThreadStatic<T>`: Windows では値を
  ヒープに置き、Fiber Local Storage のコールバックでスレッド終了時に破棄する
  （他のプラットフォームでは普通の `thread_local`）。
- 不変値（`std::locale::classic()` など）は `static const` にする。
- 別ライブラリ（`NcsfPlay.Native`）では生ポインタの `thread_local` にする。

**注意点**: 新しく `thread_local` を書くときは、整数・生ポインタ・それらの
`std::array`・`std::mt19937` のような**自明に破棄できる型**だけにする。
`shared_ptr` / `std::string` / `std::locale` / `std::optional<非自明>` /
コンテナは `ThreadStatic<T>` を使う。

**見つけ方**: `tools/native-audit/thread_local_dtor.py`（全 `thread_local` を列挙するので、型を見て判断）

---

## 5. GC がある前提の寿命（イベント処理中の自己解放）

**症状**: ESC メニューの SPECTATE（やクリックで閉じる項目全般）を押した瞬間に
落ちることがある。落ちなくてもヒープが壊れ、後で固まる・落ちる。

**原因**: `PauseMenuWindow::OnClosed` が最後の `shared_ptr` を手放し、ボタンの
クリック処理の**途中で**ウィンドウ・ビュー・ボタン自体が解放された。C# では GC が
スタック上の参照を見て、処理が終わるまで生かしておく。

**修正** (`cf140ee`): 最後の参照の解放を `Dispatcher` の次の周回へ回す
（`ReleaseAfterDispatch`）。

**注意点**: 「自分を閉じる」「自分を一覧から消す」処理を、自分のイベントハンドラの中から
呼ぶ場合、その呼び出しが最後の所有者を消さないか確認する。消すなら解放を遅延させる。

---

## 6. 終了時の static 破棄順

**症状**: 異常終了のログ出力中に、ログ処理自身が `0xC0000005` を繰り返す。

**原因**: 関数内 `static std::vector` のシンボル表キャッシュが exit 時に破棄された後、
`abort()` ハンドラがそれを読んだ。C# の static はプロセス終了まで生きている。

**修正** (`ffe74d2`): 終了後にも呼ばれうるキャッシュは `*new T()` で確保し、破棄しない。
障害ハンドラ内での障害は再ログしない（再入ガード）。

**注意点**: `atexit` / シグナル / 例外フィルタ / TLS コールバックから触る static は、
破棄されない形にしておく。

---

## 7. C# では無害な呼び出しが C++ では終了する

**症状**: bot がいる試合やスペクテイト中に、何の記録も残さずアプリが消える。
イベントビューアの例外コードは `0x40000015`（`STATUS_FATAL_APP_EXIT` = `abort()`）。

**原因**: bot のモーフボール照準処理にある `Debugger.Break()`。.NET ではデバッガが
付いていなければ何もしないが、C++ 版のローカル実装は MinGW に `SIGTRAP` がないため
`std::abort()` に落ちていた。

**修正** (`728aeb0`): `NativeRuntime::DebuggerBreak()` を「デバッガが付いているときだけ
止まる」にし、各ファイルの独自実装をそれに寄せた。

**注意点**: `Debugger.Break` / `Debug.Assert` / `Environment.FailFast` のような
.NET の診断 API は、リリース実行時の .NET の挙動（多くは何もしない）に合わせる。
「念のため abort」は C++ 独自の挙動になる。

---

## 8. スレッドをまたぐ共有状態（GC とアトミックな参照がない）

**症状**: アドベンチャーのドア遷移中、ワーカースレッドで `shared_ptr` 解放中に落ちる。

**原因**: `RoomEntity.StartTransition` は C# で `Task.Run` し、ワーカーが次の部屋を
組み立てる間もメインスレッドはフレームを回す。C# では参照の読み書きがアトミックで、
GC が解放済みオブジェクトを生かすので壊れない。C++ の `shared_ptr` /
`unordered_map` / `vector` を同時に触ると未定義動作。

**修正** (`c31e638`): `NativeRuntime/System/SceneGate` の `recursive_mutex` を、
メインスレッドのステップ・描画とワーカーの処理で保持する。ワーカーがメインスレッドの
初期化を待つ間だけ手放す。

**注意点**: C# で別スレッドから触っている参照フィールドは、`AtomicSharedPtr`
（`NativeRuntime/System/AtomicSharedPtr.hpp`）かロックで守る。

---

## 9. 行列の掛け順・OpenTK の規約

**症状**: マルチプレイで、自分がいるエリア以外の部屋パーツが描画されない。

**原因**: C# は `OpenTK.Mathematics.Matrix4` の `operator*`（行ベクトル規約）を使うが、
C++ 側で `Matrix::Multiply44` を使っていて、掛ける順序・転置が違った。ポータルの
可視判定がずれ、隣の部屋が常にカリングされていた。

**修正** (`cc1f2af`): `Matrix4` に OpenTK と同じ `operator*` を定義し、C# が
`a * b` と書いている所はすべてそれにした（C# は `Multiply44` を使っていない）。

**注意点**: 行列・ベクトル演算は、C# が呼んでいる OpenTK の演算子・関数と同じ定義の
ものを使う。似た名前の別ヘルパーで代用しない。

---

## 10. OpenTK / GL のオーバーロード（int と float）

**症状**: アドベンチャー開始時のムービーが再生されない。

**原因**: C# は `GL.Uniform4(location, 0, 0, 0, 1)` と **int** で呼んでいて、OpenTK は
`glUniform4i` を選ぶ（vec4 のユニフォームに対してはエラーになり、何もしない）。C++ 側が
float 版を呼んでいたため、C# では起きない値の書き換えが起きていた。あわせて、C# では
ポーズ中も進む時間・ムービー更新が、C++ では止まっていた。

**修正** (`4be30e5`): int 版 `GL::Uniform4` を追加し、C# と同じオーバーロードを呼ぶ。
`UpdateTime` / `UpdateMovie` をポーズ判定の外に出した。

**注意点**: C# のリテラルの型（`0` と `0f`）でどのオーバーロードが選ばれているかまで
合わせる。OpenTK では int 版と float 版で別の GL 関数になることがある。

---

## 11. `AppContext.BaseDirectory` とカレントディレクトリ

**症状**: ランチャーのウィンドウアイコンとロゴが出ない。

**原因**: C# の `avares://` は埋め込みリソース。C++ 版は実行ファイルの横のファイルとして
読む設計だったが、探す場所がカレントディレクトリになっており、ビルドもファイルを
コピーしていなかった。

**修正** (`136b74d`): `NativeRuntime::AppContextBaseDirectory()`（実行ファイルのある
ディレクトリ）から読み、CMake でアセットを実行ファイルの横へコピーする。

**注意点**: C# の `AppContext.BaseDirectory` は `current_path()` ではない。
パス文字列は UTF-8 のまま `std::filesystem::path(std::u8string)` で渡す。

---

## 12. 標準ライブラリ実装の差（libc++）

**症状**: macOS / Android の CI だけビルドが通らない。

**原因**: libc++ に `std::atomic<std::shared_ptr>`、浮動小数の `std::from_chars`、
（フラグなしでは）`std::stop_token` / `std::jthread` がない。

**修正**: `NativeRuntime/System/AtomicSharedPtr.hpp`（なければ mutex 実装）、
`NativeRuntime/System/Charconv.hpp`（C ロケールの `strtod_l` で同じ受理範囲を再現）、
CMake の `-fexperimental-library`。

**注意点**: 新しい標準ライブラリ機能を使うときは、MSVC STL / libstdc++ / libc++ の
3つで使えるか確認する。

---

## 13. 調べたが問題がなかった項目（再調査の手間を省くため）

2026-09-24 に機械的に全体を調べ、実害のある箇所がなかったもの。

| 項目 | 方法 |
|---|---|
| C# の参照型をコピーして変更し、元に届かない | clang-query で「C# の class 型のローカル変数・値渡し引数がコピー構築されている」箇所を列挙。すべて C# でも struct の型だった |
| C# の struct を `shared_ptr` で共有してしまう | C# の struct 名 227 個について `shared_ptr<T>` を検索。0 件 |
| 基底クラスのコンストラクタから派生のオーバーライドを呼ぶ（C# は派生、C++ は基底が呼ばれる） | C# 側を走査。0 件 |
| `Dictionary` の列挙順（C# は追加順、`unordered_map` は不定） | C# で foreach している辞書5つの C++ 側はすべて順序付きコンテナ |
| ソートの安定性 | ゲーム処理のソートは整数のみ |
| ループ中に同じコンテナへ追加・削除 | 該当なし |
| 初期化されないフィールド（C# は 0 初期化） | clang-tidy `cppcoreguidelines-pro-type-member-init`。実害なし（`CollisionVolume` は memset 済み、バッファは使用前に埋まる） |
| move 済み変数の使用 | clang-tidy `bugprone-use-after-move`。`Movie.cpp` の1件は C++17 の評価順で安全 |

clang-tidy の実行例（コンパイルデータベースは clang で別途構成する）:

```bash
CC=clang CXX=clang++ cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S . -B /tmp/tidy
run-clang-tidy -p /tmp/tidy -quiet -header-filter='.*/src/MphRead.Native/.*' \
  -checks='-*,bugprone-use-after-move,bugprone-dangling-handle,cppcoreguidelines-pro-type-member-init,clang-diagnostic-unsequenced' \
  'src/MphRead.Native/'
```

---

## 14. 調査の手がかり（デバッグログ）

ランチャー右下の「デバッグログ」をオンにすると、実行ファイルの横の
`logs/FruityPrime-<日時>.log` と `-native.txt` に次が残る（Windows）。

| 行 | 意味 |
|---|---|
| `[crash] first chance 0xC0000005 ... at FruityPrime.exe+0x... 関数名+0x..` | 不正アクセスが起きた場所と呼び出し履歴。ヒープ破損などで Windows が即終了させる場合も、`-native.txt` にヒープを使わない1行が残る |
| `[crash] std::out_of_range ...` の下の `(thrown from)` | C++ 例外が**投げられた**場所（ログを書いた場所ではない） |
| `[crash] the process is going down through abort()` | `abort()` で終了した場所 |
| `[freeze] the window thread ... has not come back` | 描画ループが5秒以上止まった時のスタック。10秒ごとに再取得（場所が毎回違えばループ、同じなら待ち） |
| `[save] read / wrote / artifact ... picked up` | ストーリーセーブの読み書きとアーティファクトの記録 |

`addr2line 0x...` の値は MSYS2 で `addr2line -f -C -e FruityPrime.exe 0x...` に渡せば
ファイルと行が出る。何も残らずに消えた場合は、Windows の「信頼性モニター」で例外コードと
オフセットを見る（`0x40000015` は `abort()`、`0xC0000374` はヒープ破損、
`0xC0000409` はスタック破損）。

---

## スキャナ（`tools/native-audit/`）

```bash
tools/native-audit/run_all.sh
```

| スクリプト | 対象 |
|---|---|
| `eval_order.py` | 項目1: 評価順が未規定の場所に副作用のある呼び出しが2つ以上 |
| `name_shadow.py` | 項目2: ヘルパー関数の呼び出しがクラスに解決される |
| `slot_alias.py` | 項目3: コンテナ要素の参照を渡した先・保持中にそのコンテナを変更 |
| `thread_local_dtor.py` | 項目4: `thread_local` の一覧（型を見て判断） |

どれも文字列ベースの近似で、ヒットは「読むべき場所」、ゼロは「知っている形はない」
という意味でしかない。2026-09-24 時点の既知の無害なヒット:

- `eval_order.py`: `Q3Bsp.cpp` の `r.ReadSingle()` 群（`{}` 初期化なので左から）、
  `Archive.cpp` の固定オフセット読み、`Menu.cpp` の `ReadX(ReadLine())`（入れ子）
- `name_shadow.py`: `Utility/Compress.cpp` の `Read()`（クラス側に一致するコンストラクタがない）
- `thread_local_dtor.py`: 列挙される9件はすべて自明に破棄できる型か、対策済み
