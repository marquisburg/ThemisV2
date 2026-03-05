#include "defs.h"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

void ParsePosition(std::string line, Board& board) {
    line = line.substr(9); // Skip "position "
    
    if (line.rfind("startpos", 0) == 0) {
        board.ParseFen(START_FEN);
        if (line.length() > 8) line = line.substr(9); // Skip "startpos "
        else line = "";
    } else if (line.rfind("fen", 0) == 0) {
        line = line.substr(4); // Skip "fen "
        board.ParseFen(line.c_str());
        // Handling moves after FEN is harder because FEN can be any length.
        // We need to look for "moves" keyword.
        size_t moves_pos = line.find("moves");
        if (moves_pos != std::string::npos) {
            line = line.substr(moves_pos);
        } else {
            line = "";
        }
    }
    
    size_t moves_pos = line.find("moves");
    if (moves_pos != std::string::npos) {
        line = line.substr(moves_pos + 6); // Skip "moves "
        std::stringstream ss(line);
        std::string move_str;
        while (ss >> move_str) {
            if (move_str.length() < 4) continue;

            int from = (move_str[0] - 'a') + ((move_str[1] - '1') * 8);
            int to = (move_str[2] - 'a') + ((move_str[3] - '1') * 8);

            int promoted = 0;
            if (move_str.length() > 4) {
                char p = move_str[4];
                if (p == 'q') promoted = (board.side == WHITE ? wQ : bQ);
                else if (p == 'r') promoted = (board.side == WHITE ? wR : bR);
                else if (p == 'b') promoted = (board.side == WHITE ? wB : bB);
                else if (p == 'n') promoted = (board.side == WHITE ? wN : bN);
            }

            // Parse Move (Algebraic to Move struct)
            // We need a helper function to Parse Move String -> Move
            // For now, let's implement a simple parser or rely on GenerateMoves matching.
            
            MoveList list;
            GenerateMoves(board, list);
            int parsed = 0;
            for (int i = 0; i < list.count; i++) {
                Move m = list.moves[i];
                
                if (m.from == from && m.to == to) {
                   if (promoted) {
                       if (m.promoted == promoted) {
                           if (MakeMove(board, m, m.capture)) {
                               parsed = 1;
                               break;
                           }
                       }
                   } else {
                       if (MakeMove(board, m, m.capture)) {
                           parsed = 1;
                           break;
                       }
                   }
                }
            }
            if (!parsed) {
               // Illegal move matching?
            }
        }
    }
}

void ParseGo(std::string line, Board& board) {
    int depth = 6;
    // Simple parsing for depth
    size_t depth_pos = line.find("depth");
    if (depth_pos != std::string::npos) {
        depth = std::stoi(line.substr(depth_pos + 6));
    }
    
    // Time management (wtime, btime) - for now just use fixed depth
    
    Move best_move = SearchPosition(board, depth);
    
    // Output bestmove
    // Helper: Print Coordinate Move
    // e2e4
    int f_sq = best_move.from;
    int t_sq = best_move.to;
    
    char cols[] = "abcdefgh";
    char rows[] = "12345678";
    
    std::cout << "bestmove " << cols[f_sq%8] << rows[f_sq/8] << cols[t_sq%8] << rows[t_sq/8];
    if (best_move.promoted) {
        // ... print promo char ... q, r, b, n
        // Check promoted piece type
        int type = best_move.promoted; // wQ, bQ etc.
        // Simplified:
        if (type == wQ || type == bQ) std::cout << "q";
        if (type == wR || type == bR) std::cout << "r";
        if (type == wB || type == bB) std::cout << "b";
        if (type == wN || type == bN) std::cout << "n";
    }
    std::cout << std::endl;
}

void UciLoop() {
    std::cout << "id name ThemisV2 C++" << std::endl;
    std::cout << "id author Antigravity" << std::endl;
    std::cout << "uciok" << std::endl;
    
    Board board;
    board.ParseFen(START_FEN);
    
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "uci") {
             std::cout << "id name ThemisV2 C++" << std::endl;
             std::cout << "id author Antigravity" << std::endl;
             std::cout << "uciok" << std::endl;
        } else if (line == "isready") {
             std::cout << "readyok" << std::endl;
        } else if (line.rfind("position", 0) == 0) {
             ParsePosition(line, board);
        } else if (line.rfind("go", 0) == 0) {
             ParseGo(line, board);
        } else if (line == "quit") {
             break;
        } else if (line == "print") {
             board.Print();
        }
    }
}
