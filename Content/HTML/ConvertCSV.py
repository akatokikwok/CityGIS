import pandas as pd
import json
import math
import hashlib
import datetime
import uuid

# ================= 配置区 =================
INPUT_FILE = 'exported_subdistrict_db.csv'
OUTPUT_FILE = 'Imported_Streets.json'
# COORD_TYPE = 'GCJ02' # 房产数据通常是 GCJ02 (高德/腾讯坐标)，如果是 WGS84 请改为 'WGS84'
COORD_TYPE = 'BD09' # 数据源已经是百度坐标，直接透传，不做转换
# =========================================

# 坐标转换算法 (GCJ02 -> BD09)
# 百度地图使用 BD09，如果不转换，地图会有几百米偏移
def gcj02_to_bd09(lng, lat):
    z = math.sqrt(lng * lng + lat * lat) + 0.00002 * math.sin(lat * math.pi * 3000.0)
    theta = math.atan2(lat, lng) + 0.000003 * math.cos(lng * math.pi * 3000.0)
    bd_lng = z * math.cos(theta) + 0.0065
    bd_lat = z * math.sin(theta) + 0.006
    return bd_lng, bd_lat

def wgs84_to_bd09(lng, lat):
    # 简易处理：先假装是GCJ02转过去（精度要求不高时可用，高精度需先转GCJ再转BD）
    return gcj02_to_bd09(lng, lat) 

def convert_coordinates(geometry, from_type):
    if from_type == 'BD09': return geometry
    
    new_coords = []
    # 递归处理 GeoJSON 的坐标数组 (MultiPolygon 是 4 层嵌套: Multi -> Polygon -> Ring -> Point)
    def transform_recur(coords):
        if isinstance(coords[0], (int, float)): # 到底了 [lng, lat]
            if from_type == 'GCJ02':
                return list(gcj02_to_bd09(coords[0], coords[1]))
            else:
                return list(gcj02_to_bd09(coords[0], coords[1])) # 暂用相同算法
        else:
            return [transform_recur(c) for c in coords]

    geometry['coordinates'] = transform_recur(geometry['coordinates'])
    return geometry

# 根据字符串生成固定颜色 (保证同一个区颜色相同)
def get_color_from_str(s):
    hash_val = int(hashlib.md5(str(s).encode('utf-8')).hexdigest(), 16)
    r = (hash_val & 0xFF0000) >> 16
    g = (hash_val & 0x00FF00) >> 8
    b = hash_val & 0x0000FF
    return "#{:02x}{:02x}{:02x}".format(r, g, b)

def main():
    print(f"正在读取 {INPUT_FILE} ...")
    try:
        df = pd.read_csv(INPUT_FILE)
    except Exception as e:
        print(f"读取失败: {e}")
        return

    features = []
    
    print(f"正在转换 {len(df)} 条街道数据...")
    
    for index, row in df.iterrows():
        try:
            # 1. 解析 CSV 里的 Geometry 字符串为 JSON 对象
            raw_geo_str = row['geometry']
            geo_obj = json.loads(raw_geo_str)
            
            # 2. 坐标纠偏
            geo_obj = convert_coordinates(geo_obj, COORD_TYPE)
            
            # 3. 构造 UE5 地图引擎需要的 Properties
            # 颜色逻辑：根据 district_code (区代码) 生成颜色，这样同一个区的街道颜色一样
            color = get_color_from_str(row['district_code'])
            
            prop = {
                "id": f"street_{row['id']}",
                "name": row['name'],
                "customType": "Street",  # 类型设为街道
                "svCol": color,          # 填充颜色
                "svOp": 0.4,             # 透明度
                "svLine": False,         # 是否是线 (False=面)
                "pid": "None",           # 父级ID (暂空)
                "svTxtCol": "#FFFFFF",   # 文字颜色
                "customTag": str(row['district_code']), # 用区代码做标签
                "customHeight": 0
            }
            
            # 4. 构造 Feature
            feature = {
                "type": "Feature",
                "geometry": geo_obj,
                "properties": prop
            }
            features.append(feature)
            
        except Exception as e:
            print(f"跳过错误行 {index}: {e}")

    # 5. 构造最终的存档结构 (Wrapper)
    # 这必须符合 C++ ExecuteLoadFromFile 读取的结构
    save_file_structure = {
        "id": str(uuid.uuid4()),
        "name": "Imported_Streets",
        "desc": f"从 CSV 导入的 {len(features)} 条街道数据",
        "date": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "data": features  # 核心数据放在 data 字段
    }

    # 6. 保存
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        json.dump(save_file_structure, f, ensure_ascii=False, indent=0)
        
    print(f"✅ 转换完成！已生成: {OUTPUT_FILE}")
    print(f"⚠️ 请将此文件复制到项目的 Saved/GISData/ 文件夹中。")

if __name__ == '__main__':
    main()