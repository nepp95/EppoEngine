# Chess

A chess game for EppoEngine: you against a simple bot.

## Playing

Open `Projects/Chess/Chess.epproj` in the editor and press Play. You can also pass the path on the command line: run `EppoEditor.exe Projects/Chess/Chess.epproj` from `EppoEditor/`.

| Input | Action |
|---|---|
| Left click a piece | Select it. Blue dots mark quiet moves, red outlines mark captures. |
| Left click a marked square | Move there. Pawns always promote to a queen. |
| Right click | Cancel the selection. |
| R | Start a new game. |
| Esc (editor) | Stop play mode. |

The last move is shown with yellow tiles, and a king in check sits on a red tile. When the game ends, the losing king tips over and the result goes to the log. On a draw, both kings get an outline.

The engine detects checkmate, stalemate, the fifty-move rule, threefold repetition and insufficient material.

These fields on the `Game` entity's script control the game:

- `PlayerIsWhite`: set it to false to play Black. The camera then moves to the other side.
- `BotDepth`: how many plies the bot searches, from 1 to 5. The default is 3.
- `BotDelay`: how many seconds the bot waits before it moves.

## Layout

- `Scripts/Source/Logic/` holds the rules and the bot in plain C# with no engine types: the board, move generation, the game state and the alpha-beta bot.
- `Scripts/Source/BoardView.cs` spawns and animates the board, pieces and markers, and picks the square under the cursor.
- `Scripts/Source/ChessGame.cs` is the script on the `Game` entity. It handles input, turns, the bot running on a worker thread, and the end of the game.
- `Tools/generate_meshes.py` regenerates every mesh in `Assets/Meshes`, the mesh entries in `AssetRegistry.json`, and `Scripts/Source/Generated/MeshHandles.cs`. Entities can't be tinted, so each color is its own mesh with its own material.

Mouse picking relies on the script API from the `feature/script-viewport-picking` engine branch: `Input.GetViewportMousePosition`, `Input.CursorMode` and `CameraComponent.ScreenPointToRay`.
