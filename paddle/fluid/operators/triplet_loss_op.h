/* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#pragma once
#include <vector>
#include "paddle/fluid/framework/eigen.h"
#include "paddle/fluid/framework/op_registry.h"

namespace paddle {
namespace operators {

using Tensor = framework::Tensor;
template <typename T, int MajorType = Eigen::RowMajor,
          typename IndexType = Eigen::DenseIndex>
using EigenMatrix = framework::EigenMatrix<T, MajorType, IndexType>;

template <typename DeviceContext>
std::vector<int> GetOffsets(const Tensor* t);

template <typename DeviceContext, typename T>
class TripletLossKernel : public framework::OpKernel<T> {
 public:
  void Compute(const framework::ExecutionContext& context) const override {
    const Tensor* logits = context.Input<Tensor>("Logits");
    const Tensor* labels = context.Input<Tensor>("Label");
    Tensor* loss = context.Output<Tensor>("Loss");
    loss->mutable_data<T>(context.GetPlace());
    auto x_dims = logits->dims();
    int barch_size = x_dims[0];
    int feature_len = x_dims[1];

    // step 1: distance matrix
    Tensor distances;
    distances.mutable_data<T>(context.GetPlace(), {batch_size, batch_size});
    auto x_t = EigenMatrix<T>::From(*logits);
    auto loss_t = EigenVector<T>::From(*loss);
    auto D = EigenMatrix<T>::From(distances);

    auto blas = math::GetBlas<DeviceContext, T>(context);
    auto x_mat = math::CreateMatrixDescriptor(x_dims, 0, false);
    auto x_mat_trans = math::CreateMatrixDescriptor(x_dims, 0, true);
    blas.MatMul(x_t, x_mat, x_t, x_mat_trans, T(1), distances, T(0));

    auto D = x_t.square().sum({1}).broadcast({1, batch_size}) +
             x_t.square().sum({1}).shuffle({1, 0}).broadcast({batch_size, 1}) -
             2 * D;

    // step 2:
    float eps = 0.5;
    auto relu = [eps](float x) { return x < 0 ? eps : x + eps; };
    auto offsets = GetOffsets<DeviceContext>(labels);
    for (size_t i = 0; i < offsets.size() - 1; ++i) {
      size_t begin = offsets[i];
      size_t end = offsets[i + 1];
      for (size_t j = begin; j < end; ++j) {
        auto p_dis = D.slice({j, begin}, {1, end - begin});
        auto n_dis = D.slice({j, 0}, {1, batch_size}).reshape({batch_size, 1});
        auto n_p_sub = n_dis.broadcast({end - begin, 1}) -
                       p_dis.shuffle({1, 0}).broadcast({1, end - begin});
        auto p_p_sub = p_dis.broadcast({end - begin, 1}) -
                       p_dis.shuffle({1, 0}).broadcast({1, end - begin});
        loss_t[j] =
            n_p_sub.unaryExpr(relu).sum() - p_p_sub.unaryExpr(relu).sum();
      }
    }
  }
};

template <typename DeviceContext, typename T>
class TripletLossGradKernel : public framework::OpKernel<T> {
 public:
  void Compute(const framework::ExecutionContext& context) const override {
    const Tensor* loss = context.Input<Tensor>(framework::GradVarName("Loss"));
    const Tensor* labels = context.Input<Tensor>("Label");
    const Tensor* logits = context.Input<Tensor>("Logits");
    auto x_dims = logits->dims();
    int barch_size = x_dims[0];
    int feature_len = x_dims[1];

    Tensor* d_logits = context.Output<Tensor>(framework::GradVarName("Logits"));
    d_logits->mutable_data<T>(context.GetPlace());

    Tensor d_distances;
    distances.mutable_data<T>(context.GetPlace(), {batch_size, batch_size});
    auto x_t = EigenMatrix<T>::From(*logits);
    auto d_x_t = EigenMatrix<T>::From(*d_logits);
    auto d_loss_t = EigenVector<T>::From(*d_loss);
    auto d_D = EigenMatrix<T>::From(d_distances);

    // step 1: get d_Distances
    auto relu_grad = [](float x) { return x < 0 ? 0 : 1; };
    auto offsets = GetOffsets<DeviceContext>(labels);
    for (size_t i = 0; i < offsets.size() - 1; ++i) {
      size_t begin = offsets[i];
      size_t end = offsets[i + 1];
      size_t pos_num = end - begin;
      size_t neg_num = batch_szie - pos_num;
      for (size_t j = begin; j < end; ++j) {
        d_n_p_sub_relu_sum = d_loss_t[j];
        d_p_p_sub_relu_sum = d_loss_t[j]* -1 d_p_p_sub_relu =
            d_p_p_sub_relu_sum.broadcast({(end - begin), (end - begin)})
                d_p_p_sub = d_p_p_sub.unaryExpr(
                relu_grad)* d_p_p_sub_relu d_n_p_sub_relu =
                d_n_p_sub_relu_sum.broadcast(
                    {(end - begin), (batch_size - (end - begin))}) d_n_p_sub =
                    d_n_p_sub.unaryExpr(relu_grad)* d_n_p_sub_relu d_p_dis_bc =
                        d_p_p_sub d_p_dis_trans_bc_1 =
                            d_p_p_sub* -1 d_p_dis_trans =
                                d_p_dis_trans_bc_1.sum(1) d_p_dis =
                                    p_dis_bc.sum(0) d_p_dis +=
            d_p_dis_trans.shuffle({1, 0}) d_n_dis_bc =
                d_n_p_sub d_p_dis_trans_bc_0 += d_n_p_sub* -1 d_p_dis_trans +=
            d_p_dis_trans_bc_0.sum(1) d_p_dis +=
            d_p_dis_trans.shuffle({1, 0}) d_D.slice({j, 0}, {1, batch_size}) =
                d_n_dis_bc.sum(0) d_D.slice({j, begin}, {1, end - begin}) =
                    d_p_dis
      }
    }

    d_x_t_2_sum_bc = d_D d_x_t_2_sum_trans_bc = d_D d_AB = d_D* -2

        d_x_t_2_sum_trans = d_x_t_2_sum_trans_bc.sum(0) d_x_t_2_sum =
            d_x_t_2_sum_trans.shuffle({1, 0})

                d_x_t_2_sum += d_x_t_2_sum_bc.sum(1) d_x_t_2 =
                d_x_t_2_sum.broadcast({1, feature_len}) d_x_t = 2 * x_t* d_x_t_2

                    d_x_t += d_AB* x_t_trans d_x_t_trans = d_AB* x_t.T d_x_t +=
        d_x_t_trans.T
  }
};

}  // namespace operators
}  // namespace paddle