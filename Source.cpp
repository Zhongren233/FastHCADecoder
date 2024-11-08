//--------------------------------------------------
// インクルード
//--------------------------------------------------
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <filesystem>  // 用于文件和目录操作
#include "clHCA.h"
#include "HCADecodeService.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

//--------------------------------------------------
// メイン
//--------------------------------------------------
int main(int argc, char *argv[]) {

    // コマンドライン解析
    char *directoryPath = NULL;
    char *filenameOut = NULL;
    float volume = 1;
    unsigned int ciphKey1 = 0xBC731A85;
    unsigned int ciphKey2 = 0x0002B875;
    unsigned int subKey = 0;
    int mode = 16;
    int loop = 0;
    unsigned int threads = 0;
    bool info = false;
    bool decrypt = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' || argv[i][0] == '/') {
            switch (argv[i][1]) {
            case 'd':if (i + 1 < argc) { directoryPath = argv[++i]; } break;
            case 'o':if (i + 1 < argc) { filenameOut = argv[++i]; } break;
            case 'v':volume = (float)atof(argv[++i]); break;
            case 'a':if (i + 1 < argc) { ciphKey1 = strtoul(argv[++i], NULL, 16); } break;
            case 'b':if (i + 1 < argc) { ciphKey2 = strtoul(argv[++i], NULL, 16); } break;
            case 's':if (i + 1 < argc) { subKey = strtoul(argv[++i], NULL, 16); } break;
            case 'm':if (i + 1 < argc) { mode = atoi(argv[++i]); } break;
            case 'l':if (i + 1 < argc) { loop = atoi(argv[++i]); } break;
            case 't':if (i + 1 < argc) { threads = atoi(argv[++i]); } break;
            case 'i':info = true; break;
            case 'c':decrypt = true; break;
            }
        }
    }

    // 检查输入目录是否存在
    if (!directoryPath) {
        printf("Error: ディレクトリパスを指定してください。\n");
        return -1;
    }

    // 初始化解码服务
    HCADecodeService dec{ threads };
    std::vector<std::pair<std::string, std::pair<void*, size_t>>> fileslist;

    // 遍历目录中的所有HCA文件
    for (const auto &entry : std::filesystem::directory_iterator(directoryPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".hca") {
            std::string inputFilePath = entry.path().string();
            printf("%s を処理中...\n", inputFilePath.c_str());

            // 设置输出文件名
            std::string outputFilePath;
            if (filenameOut) {
                outputFilePath = filenameOut;
            } else {
                outputFilePath = inputFilePath;
                outputFilePath.replace(outputFilePath.find(".hca"), 4, ".wav");
            }

            // 处理模式
            if (info) {
                printf("%s のヘッダ情報\n", inputFilePath.c_str());
                clHCA hca(0, 0);
                hca.PrintInfo(inputFilePath.c_str());
                printf("\n");
            } else if (decrypt) {
                printf("%s を復号化中...\n", inputFilePath.c_str());
                clHCA hca(ciphKey1, ciphKey2, subKey);
                if (!hca.Decrypt(inputFilePath.c_str())) {
                    printf("Error: 復号化に失敗しました。\n");
                }
            } else {
                printf("%s をデコード中...\n", inputFilePath.c_str());
                auto wavout = dec.decode(inputFilePath.c_str(), 0, ciphKey1, ciphKey2, subKey, volume, mode, loop);
                if (!wavout.first) {
                    printf("Error: デコードに失敗しました。\n");
                } else {
                    fileslist.emplace_back(outputFilePath, wavout);
                }
            }
        }
    }

    // 将解码后的数据写入文件
    for (auto &file : fileslist) {
        printf("%s を書き込み中...\n", file.first.c_str());
        FILE* outfile = fopen(file.first.c_str(), "wb");
        if (!outfile) {
            printf("Error: WAVEファイルの作成に失敗しました。\n");
            dec.cancel_decode(file.second.first);
        } else {
            dec.wait_on_request(file.second.first);
            fwrite(file.second.first, 1, file.second.second, outfile);
            fclose(outfile);
        }
        operator delete(file.second.first);
    }

    return 0;
}
