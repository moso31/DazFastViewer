  std::string selected_group_;
  runtime::TransformValues group_value(const std::string &id) const {
    auto found=snapshot_.group_transforms.find(id);return found==snapshot_.group_transforms.end()?runtime::TransformValues{}:found->second;
  }
  void set_group_value(const std::string &id,runtime::TransformValues value) {
    if(loading_||!document_||!group_node(*document_,id))return;
    try{runtime::validate_transform(value);if(value==group_value(id))return;auto edit=history_edit(QStringLiteral("修改组变换"));if(value==runtime::TransformValues{})snapshot_.group_transforms.erase(id);else snapshot_.group_transforms[id]=value;send();}
    catch(const std::exception &e){statusBar()->showMessage(text(e.what()),5000);QTimer::singleShot(0,this,[this]{if(document_&&!selected_group_.empty())bind_group();});}
  }
  void bind_group() {
    const auto *node=group_node(*document_,selected_group_);if(!node)return;
    const auto id=selected_group_;selection_->setText(text(node->label.empty()?id:node->label));parameters_->bind(nullptr,nullptr);
    for(auto *spin:transform_)spin->setEnabled(false);
    std::vector<ParameterControl> controls;
    for(int i=0;i<10;++i){ParameterControl c;c.id="group/"+std::to_string(i);c.label=i==9?"Scale（%）":std::string(1,"XYZ"[i%3])+(i<3?" Translate（厘米）":i<6?" Rotate（度）":" Scale（%）");c.group=i<3?"/General/Transforms/Translation":i<6?"/General/Transforms/Rotation":"/General/Transforms/Scale";
      c.minimum=i==9?.01:-1e9;c.maximum=1e9;c.slider_minimum=i<3?-200:i<6?-180:1;c.slider_maximum=i<3?200:i<6?180:300;c.step=i<6?.1:1;c.float_backed=i!=9;
      const auto component=[](ir::Vec3 v,int axis){return axis==0?v.x:axis==1?v.y:v.z;};
      const double base=i==9?node->general_scale:i<3?component(node->translation_cm,i):i<6?component(node->rotation_degrees,i-3):component(node->scale,i-6);
      c.initial=i>=6?base*100:base;
      c.read=[this,id,i,base,component]{const auto v=group_value(id);return i==9?v.general_scale*base*100:i<3?component(v.translation_cm,i)+base:i<6?component(v.rotation_degrees,i-3)+base:component(v.scale,i-6)*base*100;};
      c.write=[this,id,i,base](double shown){auto v=group_value(id);if(i==9){if(base==0)return;v.general_scale=shown/(base*100);}else {auto &xyz=i<3?v.translation_cm:i<6?v.rotation_degrees:v.scale;if(i>=6&&base==0)return;const auto x=float(i<6?shown-base:shown/(base*100));(i%3==0?xyz.x:i%3==1?xyz.y:xyz.z)=x;}set_group_value(id,v);};controls.push_back(std::move(c));
    }
    parameters_->bind_controls(std::move(controls));
  }
