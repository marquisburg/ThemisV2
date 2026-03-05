import chess
import os
from flask import Flask, render_template, request, jsonify
from src.engine_wrapper import EngineWrapper
from src.evaluation import evaluate_board

app = Flask(__name__)

# Global game state (single session for simplicity)
game_state = {
    "board": None,
    "bot": None,
    "user_color": None,
    "mode": "play",
}


def get_engine_path():
    """Resolve engine path - use standard build (ThemisEngine.exe)."""
    base = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(base, "cpp_engine", "ThemisEngine.exe")
    return path


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/new-game", methods=["POST"])
def new_game():
    data = request.get_json() or {}
    user_color = data.get("userColor", "white").lower()
    user_color = chess.WHITE if user_color in ("white", "w") else chess.BLACK
    mode = str(data.get("mode", "play")).lower()
    if mode not in ("play", "analysis"):
        mode = "play"

    game_state["board"] = chess.Board()
    game_state["user_color"] = user_color
    game_state["mode"] = mode

    result = {
        "fen": game_state["board"].fen(),
        "botMove": None,
        "gameOver": False,
        "evalCpWhite": evaluate_board(game_state["board"]),
        "mode": mode,
    }

    # In play mode, if user plays black, bot moves first.
    if mode == "play" and user_color == chess.BLACK:
        move = game_state["bot"].play(game_state["board"], quiet=True)
        if move:
            game_state["board"].push(move)
            result["fen"] = game_state["board"].fen()
            result["botMove"] = move.uci()
            result["evalCpWhite"] = evaluate_board(game_state["board"])
        result["gameOver"] = game_state["board"].is_game_over()
        if result["gameOver"]:
            result["outcome"] = str(game_state["board"].outcome()) if game_state["board"].outcome() else None

    return jsonify(result)


@app.route("/api/move", methods=["POST"])
def make_move():
    data = request.get_json()
    if not data or game_state["board"] is None:
        return jsonify({"error": "No active game"}), 400

    board = game_state["board"]
    user_color = game_state["user_color"]

    if board.turn != user_color:
        return jsonify({"error": "Not your turn"}), 400

    from_sq = data.get("from")
    to_sq = data.get("to")
    uci = data.get("uci")

    try:
        if uci:
            move = chess.Move.from_uci(uci)
        elif from_sq and to_sq:
            from_idx = chess.parse_square(from_sq)
            to_idx = chess.parse_square(to_sq)
            move = chess.Move(from_idx, to_idx)
            if data.get("promotion"):
                pt = chess.Piece.from_symbol(data["promotion"].upper()).piece_type
                move = chess.Move(from_idx, to_idx, promotion=pt)
        else:
            return jsonify({"error": "Invalid move format"}), 400

        if move not in board.legal_moves:
            return jsonify({"error": "Illegal move", "fen": board.fen()}), 400

        board.push(move)
        user_move_uci = move.uci()

        result = {
            "fen": board.fen(),
            "gameOver": board.is_game_over(),
            "botMove": None,
            "userMove": user_move_uci,
            "evalCpWhite": evaluate_board(board),
        }

        if board.is_game_over():
            result["outcome"] = str(board.outcome()) if board.outcome() else None
            return jsonify(result)

        # Bot auto-plays best move
        bot_move = game_state["bot"].play(board, quiet=True)
        if bot_move:
            board.push(bot_move)
            result["fen"] = board.fen()
            result["botMove"] = bot_move.uci()
            result["gameOver"] = board.is_game_over()
            result["evalCpWhite"] = evaluate_board(board)
            if board.is_game_over():
                result["outcome"] = str(board.outcome()) if board.outcome() else None

        return jsonify(result)

    except (ValueError, KeyError) as e:
        return jsonify({"error": str(e)}), 400


@app.route("/api/undo", methods=["POST"])
def undo_move():
    """Undo the last move(s): takes back the last full turn (user + bot)."""
    if game_state["board"] is None:
        return jsonify({"error": "No active game"}), 400

    board = game_state["board"]
    stack = board.move_stack

    if len(stack) == 0:
        return jsonify({"error": "Nothing to undo"}), 400

    # Undo last full turn: user move + bot move (2 plies)
    pops = min(2, len(stack))
    for _ in range(pops):
        board.pop()

    last_move_uci = None
    if board.move_stack:
        m = board.move_stack[-1]
        last_move_uci = m.uci()

    result = {
        "fen": board.fen(),
        "gameOver": board.is_game_over(),
        "evalCpWhite": evaluate_board(board),
        "undoneCount": pops,
        "lastMove": last_move_uci,
    }
    return jsonify(result)


@app.route("/api/import-game", methods=["POST"])
def import_game():
    """Import a game from space-separated UCI moves (e.g. 'e2e4 e7e5 g1f3')."""
    data = request.get_json() or {}
    moves_str = data.get("moves", "").strip()
    if not moves_str:
        return jsonify({"error": "No moves provided"}), 400

    user_color = data.get("userColor", "white").lower()
    user_color = chess.WHITE if user_color in ("white", "w") else chess.BLACK

    tokens = moves_str.split()
    board = chess.Board()
    applied = []
    last_error = None

    for i, token in enumerate(tokens):
        uci = token.strip().lower()
        if len(uci) < 4:
            continue
        try:
            move = chess.Move.from_uci(uci)
            if move not in board.legal_moves:
                last_error = f"Illegal move at #{i + 1}: {uci}"
                break
            board.push(move)
            applied.append(uci)
        except ValueError:
            last_error = f"Invalid move at #{i + 1}: {uci}"
            break

    if last_error and not applied:
        return jsonify({"error": last_error}), 400

    game_state["board"] = board
    game_state["user_color"] = user_color
    game_state["mode"] = "play"

    result = {
        "fen": board.fen(),
        "moves": applied,
        "gameOver": board.is_game_over(),
        "evalCpWhite": evaluate_board(board),
        "botMove": None,
    }
    if board.is_game_over() and board.outcome():
        result["outcome"] = str(board.outcome())
    if last_error:
        result["warning"] = last_error

    # If it's engine's turn and game not over, play engine move
    if (
        not board.is_game_over()
        and game_state["bot"]
        and board.turn != user_color
    ):
        bot_move = game_state["bot"].play(board, quiet=True)
        if bot_move:
            board.push(bot_move)
            result["fen"] = board.fen()
            result["botMove"] = bot_move.uci()
            result["gameOver"] = board.is_game_over()
            result["evalCpWhite"] = evaluate_board(board)
            if board.is_game_over() and board.outcome():
                result["outcome"] = str(board.outcome())

    return jsonify(result)


@app.route("/api/restore", methods=["POST"])
def restore_game():
    """Restore game state from FEN (e.g. after page refresh)."""
    data = request.get_json() or {}
    fen = data.get("fen", "").strip()
    if not fen:
        return jsonify({"error": "No FEN provided"}), 400

    try:
        board = chess.Board(fen)
    except ValueError:
        return jsonify({"error": "Invalid FEN"}), 400

    user_color = data.get("userColor", "white").lower()
    user_color = chess.WHITE if user_color in ("white", "w") else chess.BLACK

    game_state["board"] = board
    game_state["user_color"] = user_color
    game_state["mode"] = "play"

    result = {
        "fen": board.fen(),
        "gameOver": board.is_game_over(),
        "evalCpWhite": evaluate_board(board),
    }
    if board.is_game_over() and board.outcome():
        result["outcome"] = str(board.outcome())
    return jsonify(result)


@app.route("/api/legal-moves", methods=["GET"])
def legal_moves():
    """Return legal moves from a given square for the current game position."""
    if game_state["board"] is None:
        return jsonify({"moves": []})
    from_sq = request.args.get("from")
    if not from_sq or len(from_sq) != 2:
        return jsonify({"moves": []})
    try:
        from_idx = chess.parse_square(from_sq)
        board = game_state["board"]
        moves = [m.uci()[2:4] for m in board.legal_moves if m.from_square == from_idx]
        return jsonify({"moves": moves})
    except ValueError:
        return jsonify({"moves": []})


def main():
    engine_path = get_engine_path()
    bot = EngineWrapper(engine_path)

    if not bot.start():
        print("Failed to start C++ engine. Exiting.")
        print(f"Expected engine at: {engine_path}")
        return

    game_state["bot"] = bot

    try:
        print("Themis Chess Engine - Web UI")
        print("Open http://localhost:5000 in your browser")
        app.run(host="0.0.0.0", port=5000, debug=False, use_reloader=False)
    finally:
        bot.stop()


if __name__ == "__main__":
    main()
