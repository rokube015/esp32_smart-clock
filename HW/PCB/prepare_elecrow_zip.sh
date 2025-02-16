#!/bin/bash

# 作業ディレクトリ
PCB_DIR="GBR"

# PCBディレクトリが存在するか確認
if [ ! -d "$PCB_DIR" ]; then
    echo "Error: $PCB_DIR directory not found!"
    exit 1
fi

# 変換マッピング（拡張子を大文字に統一）
declare -A rename_map=(
    ["B_Cu.gbl"]="GBL.GBL"      # 半田面(裏面)パターン
    ["B_Mask.gbs"]="GBS.GBS"    # 半田面(裏面)レジスト
    ["B_Silkscreen.gbo"]="GBO.GBO"  # 半田面(裏面)のシルク印刷
    ["drl"]="TXT.TXT"           # ドリル穴情報
    ["Edge_Cuts.gm1"]="GML.GML" # 外形情報
    ["F_Cu.gtl"]="GTL.GTL"      # 部品面(表面)パターン
    ["F_Mask.gts"]="GTS.GTS"    # 部品面(表面)レジスト
    ["F_Silkscreen.gto"]="GTO.GTO"  # 部品面(表面)のシルク印刷
)

# GBRディレクトリ内のファイルをリネーム
for old_suffix in "${!rename_map[@]}"; do
    for file in "$PCB_DIR"/*-"$old_suffix"; do
        # ファイルが存在するかチェック
        [ -e "$file" ] || continue

        # 新しいファイル名を作成
        new_file="${file/-$old_suffix/-${rename_map[$old_suffix]}}"

        # リネーム実行
        mv "$file" "$new_file"
        echo "Renamed: $(basename "$file") → $(basename "$new_file")"
    done
done

# "-job.gbrjob" ファイルを削除
for job_file in "$PCB_DIR"/*-job.gbrjob; do
    if [ -e "$job_file" ]; then
        rm "$job_file"
        echo "Deleted: $(basename "$job_file")"
    fi
done

# ZIPファイル名を設定（拡張子は小文字 .zip）
ZIP_NAME="ESP32_SmartClock_GBR_ELECROW.zip"

# ZIPファイルを作成
zip -r "$ZIP_NAME" "$PCB_DIR"

echo "ZIP file created: $ZIP_NAME"
